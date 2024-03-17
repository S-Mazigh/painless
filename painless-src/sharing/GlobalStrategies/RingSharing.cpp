#ifndef NDIST

#include "RingSharing.h"
#include "clauses/ClauseManager.h"
#include "utils/Parameters.h"
#include "utils/MpiTags.h"
#include "utils/Logger.h"
#include "painless.h"
#include <random>

RingSharing::RingSharing(int id, GlobalDatabase *g_base) : GlobalSharingStrategy(id, g_base)
{
    b_filter_send = new BloomFilter();
    b_filter_received = new BloomFilter();

    this->totalSize = Parameters::getIntParam("gshr-lit", 200 * Parameters::getIntParam("c", 28));
    requests_sent = false;
}

RingSharing::~RingSharing()
{
    if (!Parameters::getBoolParam("gtest"))
    {
        MPI_Status status;
        // waiting to receive remaining end messages just in case
        MPI_Cancel(&end_request_left);
        MPI_Cancel(&end_request_right);
        MPI_Cancel(&send_request_left);
        MPI_Cancel(&send_request_right);
    }

    delete b_filter_received;
    delete b_filter_send;
}

bool RingSharing::initMpiVariables()
{
    if (world_size < 2)
    {
        LOG(0, "[Ring] I am alone or MPI was not initialized , no need for distributed mode, initialization aborted.\n");

        return false;
    }

    right_neighbor = (mpi_rank - 1 + world_size) % world_size;
    left_neighbor = (mpi_rank + 1) % world_size;

    LOG(1, "\t[RingSharing %d] left: %d, right: %d\n", mpi_rank, left_neighbor, right_neighbor);

    // For End Management
    if (MPI_Irecv(&receivedFinalResult, 1, MPI_INT, left_neighbor, MYMPI_END, MPI_COMM_WORLD, &end_request_left) != MPI_SUCCESS)
    {
        LOG(0, "Error at MPI_Irecv left of RingSharing thread!!\n");
    }
    if (MPI_Irecv(&receivedFinalResult, 1, MPI_INT, right_neighbor, MYMPI_END, MPI_COMM_WORLD, &end_request_right) != MPI_SUCCESS)
    {
        LOG(0, "Error at MPI_Irecv right of RingSharing thread!!\n");
    }

    // Bootstrap send requests: otherwise send_flags will not be true for the first sharing round
    MPI_Isend(nullptr, 0, MPI_INT, left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_left);
    MPI_Isend(nullptr, 0, MPI_INT, right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_right);
    receivedFinalResult = UNKNOWN;

    return true;
}

bool RingSharing::testIntegrity()
{
    // GlobalDatabase test_database(-1);
    // GlobalDatabase *save = this->globalDatabase;
    // this->globalDatabase = &test_database;
    // generate random clauses
    std::random_device dev;
    std::mt19937 rng(dev());
    std::uniform_int_distribution<std::mt19937::result_type> distLit(1, 3000);
    std::uniform_int_distribution<std::mt19937::result_type> distClsSize(1, 20);

    int nb_clauses = 30;
    int clause_size;
    int lbd;
    int lit;
    ClauseExchange *p_cls;
    std::vector<ClauseExchange *> v_cls;
    std::vector<ClauseExchange *> deserialized_cls;
    std::vector<ClauseExchange *> deserialized_cls_two;
    std::vector<int> serialized_v_cls;
    std::vector<int> cls;

    for (int i = 0; i < nb_clauses; i++)
    {
        cls.clear();
        clause_size = distClsSize(rng);
        lbd = distClsSize(rng) % 10 + 1; // 1---10
        for (uint j = 0; j < clause_size; j++)
        {
            lit = distLit(rng);
            lit = (lit % 2 == 0) ? lit : lit * -1;
            cls.push_back(lit);
        }
        p_cls = ClauseManager::initClause(std::move(cls), lbd);
        globalDatabase->importClause(p_cls);
        v_cls.push_back(p_cls);
    }

    serializeClauses(serialized_v_cls);

    deserializeClauses(serialized_v_cls);

    globalDatabase->exportClauses(deserialized_cls);        // get all received clauses
    globalDatabase->getClausesToSend(deserialized_cls_two); // get the clauses added to send

    if (deserialized_cls.size() != v_cls.size() || deserialized_cls.size() != nb_clauses)
    {
        LOG(0, "\t[Ring %d][Test] Error in size\n", mpi_rank);
        LOG(0, "\t\t (orig) %d vs (des) %d : gen %d\n", v_cls.size(), deserialized_cls.size(), nb_clauses);
        return false;
    }
    // all clauses of deserialized_cls must exist in v_cls and vice versa (the order will surely be different)
    bool exists;
    for (int i = 0; i < nb_clauses; i++)
    {
        exists = false;
        p_cls = deserialized_cls[i];
        for (int j = 0; j < nb_clauses; j++)
        {
            if (p_cls->lits == v_cls[j]->lits && p_cls->lbd == v_cls[j]->lbd)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            LOG(0, "\t[Ring %d][Test] Error in the clause %d of deserialize_cls\n", mpi_rank, i);
            return false;
        }
    }

    for (int i = 0; i < nb_clauses; i++)
    {
        exists = false;
        p_cls = v_cls[i];
        for (uint j = 0; j < nb_clauses; j++)
        {
            if (p_cls->lits == deserialized_cls[j]->lits && p_cls->lbd == deserialized_cls[j]->lbd)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            LOG(0, "\t[Ring %d][Test] Error in the clause %d of v_cls \n", mpi_rank, i);
            return false;
        }

        // since I emptied the clausesToSend, the clauses in clauseToSend and receivedClauses must be the same pointers
        for (int i = 0; i < nb_clauses; i++)
        {
            p_cls = deserialized_cls_two[i];
            if (p_cls != deserialized_cls[i])
            {
                LOG(0, "\t[Ring %d][Test Send vs Receive] Error in the clause %d of deserialize_cls\n", mpi_rank, i);
                return false;
            }
        }
    }
    return true;
}

bool RingSharing::doSharing()
{
    MPI_Status status;

    // Ending Management
    int end_flag_left;
    int end_flag_right;
    MPI_Request end_request;
    bool receivedEnding;

    // Sharing Management
    int send_flag_left;
    int send_flag_right;
    int clauses_flag;
    int received_size;

    // Check If my neighbors sent me an ending notification
    MPI_Test(&end_request_left, &end_flag_left, &status);
    if (end_flag_left)
    {
        LOG(1, "\t[RingSharing %d] Ending received from left %d (%d)\n", mpi_rank, status.MPI_SOURCE, receivedFinalResult);
        finalResult = receivedFinalResult;
        globalEnding = true;
    }

    MPI_Test(&end_request_right, &end_flag_right, &status);
    if (end_flag_right)
    {
        LOG(1, "\t[Gstart %d] Ending received from right %d (%d)\n", mpi_rank, status.MPI_SOURCE, receivedFinalResult);
        finalResult = receivedFinalResult;
        globalEnding = true;
    }

    // Send Ending if globalEnding; i don't care from which i received end, since he will wait for my message.
    if (globalEnding && !requests_sent)
    {
        LOG(0, "\t[RingSharing %d] It is the end for me. Will notify %d and %d.\n", mpi_rank, left_neighbor, right_neighbor);
        // I don't care about send_request.
        MPI_Isend(&finalResult, 1, MPI_INT, left_neighbor, MYMPI_END, MPI_COMM_WORLD, &end_request);
        MPI_Isend(&finalResult, 1, MPI_INT, right_neighbor, MYMPI_END, MPI_COMM_WORLD, &end_request);
        requests_sent = true;
        // can break since no blocking recv is used
        return true;
    }

    // Test my two neighbors
    MPI_Test(&send_request_left, &send_flag_left, &status);
    MPI_Test(&send_request_right, &send_flag_right, &status);

    // Get clauses to send if at least one message was received
    if (send_flag_left || send_flag_right)
    {
        tmp_serializedClauses.clear();

        gstats.sharedClauses += serializeClauses(tmp_serializedClauses);
    }

    // Send to my two neighbors my clauses
    // send only if previous message was sent
    if (send_flag_left)
    {
        clausesToSendSerializedLeft.clear();
        clausesToSendSerializedLeft = tmp_serializedClauses; // copy before send
        MPI_Isend(&clausesToSendSerializedLeft[0], clausesToSendSerializedLeft.size(), MPI_INT, left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_left);
        gstats.messagesSent++;
        LOG(1, "\t[RingSharing %d] Sent a message of size %d to my left neighbor %d\n", mpi_rank, clausesToSendSerializedLeft.size(), left_neighbor);
    }

    // send only if previous message was sent
    if (send_flag_right)
    {
        clausesToSendSerializedRight.clear();
        clausesToSendSerializedRight = tmp_serializedClauses; // copy before send
        MPI_Isend(&clausesToSendSerializedRight[0], clausesToSendSerializedRight.size(), MPI_INT, right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &send_request_right);
        gstats.messagesSent++;
        LOG(1, "\t[RingSharing %d] Sent a message of size %d to my right neighbor %d\n", mpi_rank, clausesToSendSerializedRight.size(), right_neighbor);
    }

    // Check if my neighbors sent me any clauses
    MPI_Iprobe(left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &clauses_flag, &status);
    if (clauses_flag)
    {
        MPI_Get_count(&status, MPI_INT, &received_size);
        receivedClauses.resize(received_size);
        MPI_Recv(&receivedClauses[0], received_size, MPI_INT, left_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        deserializeClauses(receivedClauses);
        LOG(1, "\t[RingSharing %d] Received a message of size %d from my left neighbor %d\n", mpi_rank, received_size, left_neighbor);
    }

    MPI_Iprobe(right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &clauses_flag, &status);
    if (clauses_flag)
    {
        MPI_Get_count(&status, MPI_INT, &received_size);
        receivedClauses.resize(received_size);
        MPI_Recv(&receivedClauses[0], received_size, MPI_INT, right_neighbor, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        deserializeClauses(receivedClauses);
        LOG(1, "\t[RingSharing %d] Received a message of size %d from my right neighbor %d\n", mpi_rank, received_size, right_neighbor);
    }

    return false;
}

//==============================
// Serialization/Deseralization
//==============================

// Pattern: this->idSharer 2 6 lbd 0 6 -9 3 5 lbd 0 -4 -2 lbd 0 nb_buffers_aggregated
int RingSharing::serializeClauses(std::vector<int> &serialized_v_cls)
{
    int clausesSelected = 0;
    ClauseExchange *tmp_cls;

    int dataCount = serialized_v_cls.size();

    while (dataCount < totalSize && globalDatabase->getClauseToSend(&tmp_cls))
    {
        if (dataCount + tmp_cls->size + 2 > totalSize)
        {
            LOG(1, "\t[RingSharing %d] Serialization overflow avoided, %d/%d, wanted to add %d\n", mpi_rank, dataCount, totalSize, tmp_cls->size + 2);
            globalDatabase->importClause(tmp_cls); // reinsert to clauseToSend if doesn't fit
            break;
        }

        // check with bloom filter if clause will be sent. If already sent, the clause is directly released
        if (!this->b_filter_send->contains(tmp_cls->lits))
        {
            serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->lits.begin(), tmp_cls->lits.end());
            serialized_v_cls.push_back((tmp_cls->lbd == 0) ? -1 : tmp_cls->lbd);
            serialized_v_cls.push_back(0);
            this->b_filter_send->insert(tmp_cls->lits);
            clausesSelected++;

            dataCount += (2 + tmp_cls->size);

            ClauseManager::releaseClause(tmp_cls);
        }
        else
        {
            gstats.sharedDuplicasAvoided++;
            ClauseManager::releaseClause(tmp_cls);
        }
    }

    if (dataCount > totalSize)
    {
        LOG(0, "Panic!! datacount(%d) > totalsize(%d). Deserialization will create erroenous clauses\n", dataCount, totalSize);
        exit(1);
    }
    return clausesSelected;
}

void RingSharing::deserializeClauses(std::vector<int> &serialized_v_cls)
{
    std::vector<int> tmp_cls;
    int current_cls = 0;
    int nb_clauses = 0;
    int lbd;
    const uint source_id = globalDatabase->getId();
    const uint buffer_size = serialized_v_cls.size();
    ClauseExchange *p_cls;

    for (int i = 0; i < buffer_size; i++)
    {
        if (serialized_v_cls[i] == 0)
        {
            tmp_cls.clear();
            tmp_cls.insert(tmp_cls.end(), serialized_v_cls.begin() + current_cls, serialized_v_cls.begin() + i);
            lbd = (tmp_cls.back() == -1) ? 0 : tmp_cls.back();
            tmp_cls.pop_back();

            if (!this->b_filter_received->contains(tmp_cls)) // check if received from another neighbor : worth it ?
            {
                p_cls = ClauseManager::initClause(std::move(tmp_cls), lbd, source_id);
                if (globalDatabase->addReceivedClause(p_cls))
                    gstats.receivedClauses++;
                this->b_filter_received->insert(tmp_cls);

                // To propagate only if it is the first time it is received
                if (!this->b_filter_send->contains(p_cls->lits)) // check if already sent: tmp_cls was moved
                {
                    globalDatabase->importClause(p_cls); // add to clausesToSend may not be added if > maxClauseSize in next round
                    this->b_filter_send->insert(p_cls->lits);
                }
                else
                {
                    gstats.sharedDuplicasAvoided++;
                }
            }
            else
            {
                gstats.receivedDuplicas++;
            }

            current_cls = i + 1;
        }
    }
}
#endif