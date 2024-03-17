#ifndef NDIST

#include "MallobSharing.h"
#include "clauses/ClauseManager.h"
#include "utils/Parameters.h"
#include "utils/MpiTags.h"
#include "utils/Logger.h"
#include "painless.h"
#include <cmath>
#include <algorithm>
#include <random>

MallobSharing::MallobSharing(int id, GlobalDatabase *g_base) : GlobalSharingStrategy(id, g_base)
{
   b_filter_final = new BloomFilter();

    this->maxClauseSize = Parameters::getIntParam("max-cls-size", 30);
    this->totalSize = 0;
    this->defaultSize = Parameters::getIntParam("gshr-lit", 200 * Parameters::getIntParam("c", 28));
    requests_sent = false;

    // root_end = 0;
}

MallobSharing::~MallobSharing()
{
    if (!Parameters::getBoolParam("gtest"))
    {
        if (mpi_rank == 0)
        {
            MPI_Cancel(&end_request);
        }
    }
    delete b_filter_final;
}

bool MallobSharing::initMpiVariables()
{
    if (world_size < 2)
    {
        LOG(0, "[Mallob] I am alone or MPI was not initialized, no need for distributed mode, initialization aborted.\n");
        return false;
    }
    right_child = (mpi_rank * 2 + 1 < world_size) ? mpi_rank * 2 + 1 : MPI_UNDEFINED;
    left_child = (mpi_rank * 2 + 2 < world_size) ? mpi_rank * 2 + 2 : MPI_UNDEFINED;
    nb_children = (right_child == MPI_UNDEFINED) ? 0 : (left_child == MPI_UNDEFINED) ? 1
                                                                                     : 2;
    // floor((rank-1)/2)
    father = (mpi_rank == 0) ? MPI_UNDEFINED : (mpi_rank - 1) / 2;
    LOG(1, "\t[Mallob %d] parent:%d, left: %d, right: %d\n", mpi_rank, father, left_child, right_child);

    receivedFinalResultRoot = UNKNOWN;

    // For End Management
    if (mpi_rank == 0)
    {
        if (MPI_Irecv(&receivedFinalResultRoot, 1, MPI_INT, MPI_ANY_SOURCE, MYMPI_END, MPI_COMM_WORLD, &end_request) != MPI_SUCCESS)
        {
            LOG(0, "Error at MPI_Irecv of GStrat thread!!\n");
        }
    }

    // the equation is described in mallob paper and the best value for alpha: https://doi.org/10.1007/978-3-030-80223-3_35
    // int nb_buffers_aggregated = countDescendants(world_size, mpi_rank);
    // this->totalSize = nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) * default_size;
    return true;
}

bool MallobSharing::testIntegrity()
{

    // generate random clauses
    this->totalSize = defaultSize;
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
    std::vector<std::vector<int>> serialized_v_cls(3);
    std::vector<int> cls;

    for (int k = 0; k < 3; k++)
    {
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

        serializeClauses(serialized_v_cls[k]);
        this->totalSize += serialized_v_cls[k].size();
    }

    std::vector<int> result;
    std::vector<int> result_sizes;

    mergeSerializedBuffersWithMine(serialized_v_cls, result);

    deserializeClauses(result, &GlobalDatabase::addReceivedClause);

    globalDatabase->exportClauses(deserialized_cls); // get all receveid clauses

    nb_clauses *= 3; // the size of the merge

    if (deserialized_cls.size() != v_cls.size() || deserialized_cls.size() != nb_clauses)
    {
        LOG(0, "\t[Mallob %d][Test] Error in size\n", mpi_rank);
        LOG(0, "\t\t (orig) %d vs (des) %d : gen %d\n", v_cls.size(), deserialized_cls.size(), nb_clauses);
        return false;
    }

    for (int i = 0; i < nb_clauses; i++)
    {
        result_sizes.push_back(deserialized_cls[i]->lits.size());
    }

    if (!std::is_sorted(result_sizes.begin(), result_sizes.end()))
    {
        LOG(0, "\t[Mallob %d][Test] Error in result size order\n", mpi_rank);
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
            LOG(0, "\t[Mallob %d][Test] Error in the clause %d of deserialize_cls\n", mpi_rank, i);
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
            LOG(0, "\t[Mallob %d][Test] Error in the clause %d of v_cls \n", mpi_rank, i);
            return false;
        }
    }

    return true;
}

// check if can be done with Recv_Init
bool MallobSharing::doSharing()
{
    // Ending Management
    int end_flag;
    MPI_Status status;
    // [0]: parent, [1]: right, [2]: left. If it has one child it must be a right one
    // MPI_Request send_request[nb_children + 1];
    MPI_Request send_request;
    SatResult receivedFinalResultBcast = UNKNOWN;

    // Sharing Management
    std::vector<int> my_clauses_buffer;
    int received_buffer_size = 0;
    int root_size = 0;
    int buffer_size = 0;
    int nb_buffers_aggregated = 1; // my buffer is accounted here

    if (globalEnding && !requests_sent && mpi_rank != 0) // send end only once
    {
        LOG(1, "\t[Mallob %d] It is the end, now I will send end to the root\n", mpi_rank);
        MPI_Isend(&finalResult, 1, MPI_INT, 0, MYMPI_END, MPI_COMM_WORLD, &send_request);
        requests_sent = true;
        // I cannot leave yet, need to participate in the split once.
    }

    if (mpi_rank == 0)
    {
        if (globalEnding) // to check if the root found the solution
        {
            receivedFinalResultBcast = finalResult;
            LOG(1, "\t[Mallob %d] It is the end, now I will send end to all descendants (%d)\n", mpi_rank, receivedFinalResultBcast);
        }
        else // check recv only if root didn't end
        {
            MPI_Test(&end_request, &end_flag, &status);
            if (end_flag)
            {
                LOG(1, "\t[Mallob %d] Ending received from %d (%d)\n", mpi_rank, status.MPI_SOURCE, receivedFinalResultRoot);
                globalEnding = true;
                receivedFinalResultBcast = receivedFinalResultRoot;
                // root_end = 1;
                // end arrived to the root. Know it should tell everyone
            }
        }
    }

    // Broadcast 0 or 1 to
    MPI_Bcast(&receivedFinalResultBcast, 1, MPI_INT, 0, MPI_COMM_WORLD);
    LOG(1, "\t[Mallob %d] Received finalResult = %d \n", mpi_rank, receivedFinalResultBcast);

    if (receivedFinalResultBcast != UNKNOWN)
    {
        LOG(1, "\t[Mallob %d] It is the real end: %d\n", mpi_rank, receivedFinalResultBcast);
        globalEnding = true;
        finalResult = receivedFinalResultBcast;
        return true;
    }

    // SHARING
    //========
    // send my clauses to my parent if leaf then wait of its response
    if (father != MPI_UNDEFINED && nb_children == 0)
    {
        this->totalSize = nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) * defaultSize;
        LOG(1, "\t[Mallob %d] TotalSize = %d\n", mpi_rank, totalSize);

        my_clauses_buffer.clear();

        gstats.sharedClauses += serializeClauses(my_clauses_buffer);

        my_clauses_buffer.push_back(nb_buffers_aggregated); // number of buffers aggregated

        //  I am a leaf so i will only send my clauses (no merge)
        MPI_Send(&my_clauses_buffer[0], my_clauses_buffer.size(), MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD);
        gstats.messagesSent++;

        // Now I will wait for my father's response
        MPI_Probe(father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &received_buffer_size);
        receivedClauses.resize(received_buffer_size);

        MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
    }

    // if not leaf and not root, wait for children clauses then send to parent
    if (nb_children >= 1) // has a right child wanting to share
    {
        receivedClauses.clear();

        MPI_Probe(right_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &received_buffer_size);
        receivedClauses.resize(received_buffer_size);

        MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, right_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);

        nb_buffers_aggregated += receivedClauses.back(); // get nb buffers aggregated of my right child
        receivedClauses.pop_back();                      // remove the nb_buffers_aggregated
        // add the clauses to the mergeBuffer
        buffers.push_back(std::move(receivedClauses));

        if (nb_children == 2) // has a left child wanting to share
        {
            receivedClauses.clear();

            MPI_Probe(left_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_INT, &received_buffer_size);
            receivedClauses.resize(received_buffer_size);

            MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, left_child, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);

            nb_buffers_aggregated += receivedClauses.back(); // get nb buffers aggregated of my right child
            receivedClauses.pop_back();                      // remove the nb_buffers_aggregated
            // add the clause to the mergeBuffer
            buffers.push_back(std::move(receivedClauses));
        }
    }

    // prepare my clauses (root and middle node)
    if (nb_children > 0)
    {
        this->totalSize = nb_buffers_aggregated * pow(0.875, log2(nb_buffers_aggregated)) * defaultSize;
        LOG(1, "\t[Mallob %d] TotalSize = %d(%d)\n", mpi_rank, totalSize, nb_buffers_aggregated);

        my_clauses_buffer.clear();

        gstats.sharedClauses += serializeClauses(my_clauses_buffer);

        clausesToSendSerialized.clear();

        // add my clauses to the merge buffer
        buffers.push_back(my_clauses_buffer);

        // merge then send the result or bcast (if root)
        gstats.sharedClauses += mergeSerializedBuffersWithMine(buffers, clausesToSendSerialized);
    }

    // Middle Node
    if (father != MPI_UNDEFINED && nb_children > 0)
    {
        clausesToSendSerialized.push_back(nb_buffers_aggregated); // number of buffers aggregated

        // Send to my parent the merge result
        MPI_Send(&clausesToSendSerialized[0], clausesToSendSerialized.size(), MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD);
        gstats.messagesSent++;

        // Wait for my parent's response
        // Now I will wait for my father's response
        MPI_Probe(father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
        MPI_Get_count(&status, MPI_INT, &received_buffer_size);
        receivedClauses.resize(received_buffer_size);

        MPI_Recv(&receivedClauses[0], received_buffer_size, MPI_INT, father, MYMPI_CLAUSES, MPI_COMM_WORLD, &status);
    }

    if (mpi_rank == 0)
    {
        receivedClauses = std::move(clausesToSendSerialized);
        received_buffer_size = receivedClauses.size();
    }

    // Response to my children
    if (nb_children >= 1)
    {
        MPI_Send(&receivedClauses[0], received_buffer_size, MPI_INT, right_child, MYMPI_CLAUSES, MPI_COMM_WORLD);
        gstats.messagesSent++;

        if (nb_children >= 2)
        {
            MPI_Send(&receivedClauses[0], received_buffer_size, MPI_INT, left_child, MYMPI_CLAUSES, MPI_COMM_WORLD);
            gstats.messagesSent++;
        }
    }

    // deserialize for all
    deserializeClauses(receivedClauses, &GlobalDatabase::addReceivedClause);
    return false;
}

//==============================
// Serialization/Deseralization
//==============================

// Pattern: this->idSharer 2 6 lbd 0 6 -9 3 5 lbd 0 -4 -2 lbd 0 nb_buffers_aggregated
int MallobSharing::serializeClauses(std::vector<int> &serialized_v_cls)
{
    int clausesSelected = 0;
    ClauseExchange *tmp_cls;

    // Temporary bloom filter to not serialized twice the same clause
    BloomFilter b_filter;

    int dataCount = serialized_v_cls.size(); // it is an append operation so datacount do not start from zero

    while (dataCount < totalSize && globalDatabase->getClauseToSend(&tmp_cls))
    {
        // to not overflow the sending buffer: do not add if the clause size + lbd and the 0 will cause an overflow
        if (dataCount + tmp_cls->size + 2 > totalSize)
        {
            LOG(1, "\t[Mallob %d] Serialization overflow avoided, %d/%d, wanted to add %d\n", mpi_rank, dataCount, totalSize, tmp_cls->size + 2);
            globalDatabase->importClause(tmp_cls); // reinsert if doesn't fit
            break;
        }

        // check if not received previously and is not already serialized
        if (!this->b_filter_final->contains(tmp_cls->lits) && !b_filter.contains(tmp_cls->lits))
        {
            serialized_v_cls.insert(serialized_v_cls.end(), tmp_cls->lits.begin(), tmp_cls->lits.end());
            serialized_v_cls.push_back((tmp_cls->lbd == 0) ? -1 : tmp_cls->lbd);
            serialized_v_cls.push_back(0);
            b_filter.insert(tmp_cls->lits);
            clausesSelected++;

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

    if (dataCount > totalSize)
    {
        LOG(0, "Panic!! datacount(%d) > totalsize(%d). Deserialization will create erroenous clauses\n", dataCount, totalSize);
        exit(1);
    }
    return clausesSelected;
}

void MallobSharing::deserializeClauses(std::vector<int> &serialized_v_cls, bool (GlobalDatabase::*add)(ClauseExchange *cls))
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
            // this makes a move if possible, then tmp_cls is moved in clause_init.
            // std::move(v_cls.begin() + current_cls, v_cls.begin() + i, tmp_cls.begin()); // doesn't work right know
            tmp_cls.clear();
            tmp_cls.insert(tmp_cls.end(), serialized_v_cls.begin() + current_cls, serialized_v_cls.begin() + i);
            lbd = (tmp_cls.back() == -1) ? 0 : tmp_cls.back();
            tmp_cls.pop_back();

            if (!this->b_filter_final->contains(tmp_cls))
            {
                p_cls = ClauseManager::initClause(std::move(tmp_cls), lbd, source_id);
                if ((globalDatabase->*add)(p_cls))
                    gstats.receivedClauses++;
                this->b_filter_final->insert(tmp_cls); // either added or not wanted (>maxclauseSize)
            }
            else
            {
                gstats.receivedDuplicas++;
            }

            current_cls = i + 1; // jumps the 0
        }
    }
}

int MallobSharing::mergeSerializedBuffersWithMine(std::vector<std::vector<int>> &buffers, std::vector<int> &result)
{
    uint dataCount = 0;
    uint nb_clauses = 0;
    uint lbd = 0;
    uint buffer_count = buffers.size();
    std::vector<uint> indexes(buffer_count, 0);
    std::vector<uint> buffer_sizes(buffer_count);
    std::vector<uint> current_cls(buffer_count, 0);
    std::vector<std::vector<int>> tmp_clauses;

    std::vector<std::vector<int>>::iterator min_cls;

     // Temporary bloom filter to not push twice the same clause, no need to check with b_filter_final since everyone checks at serialization
    BloomFilter b_filter;

    for (int i = 0; i < buffer_count; i++)
    {
        buffer_sizes[i] = buffers[i].size();
    }

    while (dataCount < totalSize) // while I can add data
    {
        // check if a buffer was all seen; if all buffers are seen, loop will consume tmp_clauses before breaking
        for (int k = 0; k < buffer_count; k++)
        {
            if (indexes[k] >= buffer_sizes[k]) // a buffer is all seen, erase its cell in all vectors
            {
                LOG(2, "\t[Mallob %d] Buffer at %d will be removed since it is empty\n", mpi_rank, k);
                buffers.erase(buffers.begin() + k);
                indexes.erase(indexes.begin() + k);
                buffer_sizes.erase(buffer_sizes.begin() + k);
                current_cls.erase(current_cls.begin() + k);
                buffer_count--;
                k--; // since now element at k is the next one
            }
            else // I can produce a new clause
            {
                if (buffers[k][indexes[k]] == 0) // clause finished
                {
                    tmp_clauses.emplace_back(std::vector<int>(buffers[k].begin() + current_cls[k], buffers[k].begin() + indexes[k] + 1)); // i+1 so that the zero is included since we need a serialized buffer at the end
                    current_cls[k] = indexes[k] + 1;                                                                                      // won't go out of bound since if i == size the buffer is removed
                }
                indexes[k]++;
            }
        }

        if (tmp_clauses.size() == buffer_count && buffer_count > 0 || tmp_clauses.size() > 0 && buffer_count == 0) // there is enough clauses to compare OR add the last clauses remaining
        {
            // chooses the min by size, and if the same compare the lbds
            // iterator_of_min - iterator_begin = index
            min_cls = std::min_element(tmp_clauses.begin(), tmp_clauses.end(), [](const std::vector<int> &a, const std::vector<int> &b)
                                       { return (a.size() < b.size() || (a.size() == b.size() && a.back() <= b.back())); });
            if (dataCount + min_cls->size() <= totalSize)
            {
                if (!b_filter.contains_or_insert(std::vector(min_cls->begin(), min_cls->end() - 2))) // check without the lbd and 0
                {
                    dataCount += min_cls->size();
                    result.insert(result.end(), min_cls->begin(), min_cls->end());
                    nb_clauses++;
                }
                else
                {
                    gstats.sharedDuplicasAvoided++;
                }

                min_cls->clear();
                tmp_clauses.erase(min_cls);
            }
            else
            {
                LOG(2, "Overflow avoided[1] , %d / %d \n", dataCount, totalSize);
                break;
            }
        }
        else if (tmp_clauses.size() == 0 && buffer_count == 0) // no more data to be added
            break;
    }

    // Adding remaining clauses to clausesToSend database
    if (buffer_count > 0)
    { // clauses remained
        std::vector<ClauseExchange *> v_cls;
        std::vector<int> tmp_buff;
        for (int j = 0; j < buffer_count; j++)
        {
            tmp_buff.insert(tmp_buff.end(), buffers[j].begin() + current_cls[j], buffers[j].end());
        }
        for (auto cls : tmp_clauses)
        {
            tmp_buff.insert(tmp_buff.end(), cls.begin(), cls.end());
        }
        deserializeClauses(tmp_buff, &GlobalDatabase::importClause);
    }

    return nb_clauses;
}

#endif