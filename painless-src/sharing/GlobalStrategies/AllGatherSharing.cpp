#ifndef NDIST

#include "AllGatherSharing.h"
#include "clauses/ClauseManager.h"
#include "utils/Parameters.h"
#include "utils/MpiTags.h"
#include "utils/Logger.h"
#include "painless.h"
#include <random>

AllGatherSharing::AllGatherSharing(int id, GlobalDatabase *g_base) : GlobalSharingStrategy(id, g_base)
{
    b_filter = new BloomFilter();
    this->totalSize = Parameters::getIntParam("gshr-lit", 500 * Parameters::getIntParam("c", 28));
    requests_sent = false;
}

AllGatherSharing::~AllGatherSharing()
{
    if (!Parameters::getBoolParam("gtest"))
    {

        // Test the end_request in case the winner is hanging
        int flag;
        MPI_Test(&end_request, &flag, MPI_STATUS_IGNORE);
        if (requests_sent)
        {
            for (int i = 0; i < world_size; i++)
            {
                MPI_Cancel(&sent_end_request[i]);
            }
        }
    }

    delete[] sent_end_request;
    delete b_filter;
}

bool AllGatherSharing::initMpiVariables()
{
    if (world_size < 2)
    {
        LOG(0, "[Allgather] I am alone or MPI was not initialized, no need for distributed mode, initialization aborted.\n");
        return false;
    }
    // For End Management
    if (MPI_Irecv(&receivedFinalResult, 1, MPI_INT, MPI_ANY_SOURCE, MYMPI_END, MPI_COMM_WORLD, &end_request) != MPI_SUCCESS)
    {
        LOG(0, "Error at MPI_Irecv of AllGather thread!!\n");
        exit(1);
    }
    receivedFinalResult = UNKNOWN;
    sent_end_request = new MPI_Request[world_size];
    return true;
}

bool AllGatherSharing::testIntegrity()
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
    std::vector<int> serialized_v_cls;
    std::vector<int> cls;

    for (int i = 0; i < nb_clauses; i++)
    {
        cls.clear();
        clause_size = distClsSize(rng);
        lbd = distClsSize(rng) % 10 + 1; // 1---10
        for (int j = 0; j < clause_size; j++)
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

    deserializeClauses(serialized_v_cls, totalSize, 1);

    globalDatabase->exportClauses(deserialized_cls); // get all receveid clauses

    if (deserialized_cls.size() != v_cls.size() || deserialized_cls.size() != nb_clauses)
    {
        LOG(0, "\t[AllGather %d][Test] Error in size\n", mpi_rank);
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
            LOG(0, "\t[AllGather %d][Test] Error in the clause %d of deserialize_cls\n", mpi_rank, i);
            return false;
        }
    }

    for (int i = 0; i < nb_clauses; i++)
    {
        exists = false;
        p_cls = v_cls[i];
        for (int j = 0; j < nb_clauses; j++)
        {
            if (p_cls->lits == deserialized_cls[j]->lits && p_cls->lbd == deserialized_cls[j]->lbd)
            {
                exists = true;
                break;
            }
        }
        if (!exists)
        {
            LOG(0, "\t[AllGather %d][Test] Error in the clause %d of v_cls \n", mpi_rank, i);
            return false;
        }
    }

    return true;
}

bool AllGatherSharing::doSharing()
{

    // Ending Management
    int end_flag;
    bool can_break;
    MPI_Status status;

    // Sharing Management
    int yes_comm_size;
    MPI_Comm yes_comm;
    int sizeExport;

    // CHECK IF SHOULD END 1
    //=======================
    if (globalEnding && !requests_sent) // send request only once
    {
        LOG(0, "\n[AllGather %d] It is the end, now will send to everyone\n", mpi_rank);
        // volatile int i = 0;
        // char hostname[256];
        // gethostname(hostname, sizeof(hostname));
        // printf("PID %d on %s of rank %d ready for attach\n", getpid(), hostname, mpi_rank);
        // // while (0 == i)
        // sleep(20);
        // send END to the others: cannot use Bcast since it must be called by all ranks (shouldn't be in an if statement)
        /*
        - I use Issend to not have a deadlock if more than one sends, and the synchronize permit us to break when we are sure that no one will share.
        - If two sends the messages, they will not be stuck since in the end at least one will detect the message of the other and break
        - The send_request will be tested, so that this process can finally break since every one received the message*/
        for (int i = 0; i < world_size; i++)
        {
            if (i != mpi_rank)
            {
                MPI_Issend(&finalResult, 1, MPI_INT, i, MYMPI_END, MPI_COMM_WORLD, &sent_end_request[i]);
                LOG(3, "[AllGather %d] sent end to %d\n", mpi_rank, i);
            }
            else
            {
                sent_end_request[i] = MPI_REQUEST_NULL;
            }
        }
        requests_sent = true;
        // here no break to not create a problem at Mpi_Comm_Split but this process will no participate in the sharing
    }

    // SHARING
    //========

    // At wake up, get the clauses to export and check if empty
    if (requests_sent)
    {
        // if empty, not worth it or local is ending
        this->color = MPI_UNDEFINED; // to not include this mpi process in the yes_comm
    }
    else
    {
        this->color = COLOR_YES;
    }

    LOG(3, "\t[AllGather %d] before splitting\n", mpi_rank);

    // The call must be done only when all global sharers can arrive here.
    if (MPI_Comm_split(MPI_COMM_WORLD, this->color, mpi_rank, &yes_comm) != MPI_SUCCESS)
    {
        LOG(0, "Error in spliting the MPI_COMM_WORLD by color, aborting the sharing!!\n");
        return false;
    }

    if (this->color == MPI_UNDEFINED)
    {
        LOG(1, "\t[AllGather %d] is not willing to share (%d)\n", mpi_rank);
    }
    else // do this only if member of yes_comm
    {
        MPI_Comm_set_errhandler(yes_comm, MPI_ERRORS_RETURN);

        if (MPI_Comm_size(yes_comm, &yes_comm_size) != MPI_SUCCESS)
        {
            LOG(0, "Error in getting the new COMM size, aborting the sharing!!\n");
            return false;
        }

        if (yes_comm_size < 2)
        {
            LOG(1, "\t[AllGather %d] Not enough sharers want to share their clauses\n", mpi_rank);

            MPI_Test(&end_request, &end_flag, &status); // to check if I am alone because the others ended
            if (end_flag)
            {
                LOG(1, "\t[AllGather %d] Ending received from %d\n", mpi_rank, status.MPI_SOURCE);
                finalResult = receivedFinalResult;
                globalEnding = true;
                return true;
            }
            return true; // can break and end the global sharer thread since it is doing nothing
        }
        else
        {
            LOG(2, "\t[AllGather %d] %d global sharers will share their clauses\n", mpi_rank, yes_comm_size);
        }

        // get clauses to send and serialize
        clausesToSendSerialized.clear();
        gstats.sharedClauses += serializeClauses(clausesToSendSerialized);
        // limit the receive buffer size to contain all the exported buffers
        // resize is used to really allocate the memory
        receivedClauses.clear();
        receivedClauses.resize((totalSize)*yes_comm_size);

        LOG(3, "\t[AllGather %d] before allgather\n", mpi_rank);
        //  do not clear  the receivedClause vector before allgather to no cause segmentation faults, MPI access it in a C like fashion, it overwrites using pointer arithmetic
        if (MPI_Allgather(&clausesToSendSerialized[0], totalSize, MPI_INT, &receivedClauses[0], totalSize, MPI_INT, yes_comm) != MPI_SUCCESS)
        {
            LOG(0, "Error in AllGather for yes_comm, aborting sharing !!\n");
            // return true;
        }
        gstats.messagesSent += yes_comm_size;

        // Now I have a vector of the all the gathered buffers
        deserializeClauses(receivedClauses, totalSize, yes_comm_size);
    }

    // CHECK IF SHOULD END 2
    //=======================

    MPI_Test(&end_request, &end_flag, &status);
    if (end_flag)
    {
        LOG(1, "\t[%d] Ending received from %d\n", mpi_rank, status.MPI_SOURCE);
        globalEnding = true;
        finalResult = receivedFinalResult;
        return true;
    }

    if (requests_sent) // I sent non blocking synchronous END
    {
        can_break = true;
        for (int i = 0; i < world_size; i++)
        {
            // Mpi_Test the Mpi_Request_Null will return true
            MPI_Test(&sent_end_request[i], &end_flag, MPI_STATUS_IGNORE); // will test if everyone has it so that i can break
            can_break = can_break & (bool)end_flag;                       // if at least one didn't receive, loop.
        }
        if (can_break)
        {
            LOG(2, "\t[AllGather %d] Nice, everyone ended. I can now break\n", mpi_rank);
            return true;
        }
    }

    return false;
}

//===============================
// Serialization/Deseralization
//===============================

// Pattern: this->idSharer 2 6 lbd 0 6 -9 3 5 lbd 0 -4 -2 lbd 0 0 0 0 0 0 0 0 0 0 0 0 0
int AllGatherSharing::serializeClauses(std::vector<int> &serialized_v_cls)
{
    int nb_clauses = 0;
    int dataCount = serialized_v_cls.size(); // it is an append operation so datacount do not start from zero
    ClauseExchange *tmp_cls;

    while (dataCount <= totalSize && globalDatabase->getClauseToSend(&tmp_cls)) // stops when buffer is full or no more clauses are available
    {
        if (dataCount + tmp_cls->size + 2 > totalSize - 1)
        {
            LOG(1, "\t[AllGather %d] Serialization overflow avoided, %d/%d, wanted to add %d\n", mpi_rank, dataCount, totalSize, tmp_cls->size + 2);
            globalDatabase->importClause(tmp_cls); // reinsert the clause to the database to not loose it
            break;
        }
        else
        {
            // check with bloom filter if clause will be sent. If already sent, the clause is directly released
            if (!this->b_filter->contains(tmp_cls->lits))
            {
                serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->lits.begin(), tmp_cls->lits.end());
                serialized_v_cls.push_back((tmp_cls->lbd == 0) ? -1 : tmp_cls->lbd);
                serialized_v_cls.push_back(0);
                this->b_filter->insert(tmp_cls->lits);
                nb_clauses++;

                dataCount += (2 + tmp_cls->size);
                // lbd and 0 and clause size
                // decrement ref
                ClauseManager::releaseClause(tmp_cls);
            }
            else
            {
                gstats.sharedDuplicasAvoided++;
                // std::cout << "Duplicate : " << tmp_cls->nbRefs;
                ClauseManager::releaseClause(tmp_cls);
                // std::cout << " -> " << tmp_cls->nbRefs << std::endl;
            }
        }
    }

    // fill with zero if needed
    serialized_v_cls.insert(serialized_v_cls.end(), totalSize - dataCount, 0);
    return nb_clauses;
}

void AllGatherSharing::deserializeClauses(std::vector<int> &serialized_v_cls, unsigned oneVectorSize, unsigned vectorCount)
{
    std::vector<int> tmp_cls;
    int vectorsSeen = 0;
    int current_cls = 0;
    int nb_clauses = 0;
    int lbd = -1;
    ClauseExchange *p_cls;
    int bufferSize = serialized_v_cls.size();

    for (int i = 0; i < bufferSize; i++)
    {
        if (current_cls >= bufferSize)
        {
            break;
        }
        if (serialized_v_cls[current_cls] == 0)
        { // 2 successive zeros jump to next vector if present
            ++vectorsSeen;
            LOG(1, "\t[AllGather %d] Deserialization stopped at %d for vector %d/%d\n", mpi_rank, current_cls, vectorsSeen, vectorCount);
            if (vectorsSeen >= vectorCount)
            {
                break;
            }

            current_cls = oneVectorSize * vectorsSeen;
            i = current_cls;
            continue;
        }
        if (serialized_v_cls[i] == 0)
        {
            // this makes a move if possible, then tmp_cls is moved in clause_init.
            // std::move(v_cls.begin() + current_cls, v_cls.begin() + i, tmp_cls.begin()); // doesn't work right know
            tmp_cls.clear();
            tmp_cls.insert(tmp_cls.end(), serialized_v_cls.begin() + current_cls, serialized_v_cls.begin() + i);
            lbd = (tmp_cls.back() == -1) ? 0 : tmp_cls.back();
            tmp_cls.pop_back();

            if (!b_filter->contains(tmp_cls))
            {
                p_cls = ClauseManager::initClause(std::move(tmp_cls), lbd, globalDatabase->getId());
                if (globalDatabase->addReceivedClause(p_cls))
                    gstats.receivedClauses++;
                b_filter->insert(tmp_cls); // either added or not wanted (> maxClauseSize)
            }
            else
            {
                gstats.receivedDuplicas++;
            }

            current_cls = i + 1; // jumps the 0
        }
    }
}
#endif