#include "MpiUtils.h"
#include <mpi.h>
#include "Logger.h"
#include "painless.h"
#include "protobuf/Clauses.pb.h"

#ifndef NDIST
int mpi_rank = -1;
int world_size = -1;
int mpi_winner = -1;
#else
int mpi_rank = 0;
int world_size = 0;
int mpi_winner = 0;
#endif

/* TODO: use cap'n proto instead of protobuf, but retain the chunk management */
const int CHUNK_SIZE = 10000000; // Rought estimate of 10 millions clauses

bool serializeClauses(const std::vector<simpleClause> &clauses, std::vector<std::vector<char>> &serializedChunks)
{
    Clauses protobufClauses;
    protobufClauses.set_num_clauses(clauses.size());

    int chunkIndex = 0;
    ClausesChunk *currentChunk = protobufClauses.add_chunks();
    currentChunk->set_chunk_index(chunkIndex);

    for (const auto &clause : clauses)
    {
        if (currentChunk->clauses_size() >= CHUNK_SIZE)
        {
            currentChunk = protobufClauses.add_chunks();
            currentChunk->set_chunk_index(++chunkIndex);
        }

        Clause *tempClause = currentChunk->add_clauses();
        for (auto lit : clause)
        {
            tempClause->add_literals(lit);
        }
    }

    protobufClauses.set_num_chunks(protobufClauses.chunks_size());

    for (int i = 0; i < protobufClauses.chunks_size(); ++i)
    {
        const auto &chunk = protobufClauses.chunks(i);
        size_t size = chunk.ByteSizeLong();
        std::vector<char> serializedChunk(size);
        chunk.SerializeToArray(serializedChunk.data(), size);
        serializedChunks.push_back(std::move(serializedChunk));
    }

    return true;
}

bool deserializeClauses(const std::vector<std::vector<char>> &serializedChunks, std::vector<simpleClause> &clauses)
{
    Clauses protobufClauses;
    protobufClauses.set_num_chunks(serializedChunks.size());

    for (const auto &serializedChunk : serializedChunks)
    {
        ClausesChunk chunk;
        if (!chunk.ParseFromArray(serializedChunk.data(), serializedChunk.size()))
        {
            LOGERROR("Error parsing protobuf chunk");
            return false;
        }

        protobufClauses.add_chunks()->CopyFrom(chunk);
    }

    clauses.clear();
    for (const auto &chunk : protobufClauses.chunks())
    {
        for (const auto &clause : chunk.clauses())
        {
            simpleClause tempClause;
            for (int i = 0; i < clause.literals_size(); ++i)
            {
                tempClause.push_back(clause.literals(i));
            }
            clauses.push_back(tempClause);
        }
    }

    return true;
}

bool sendFormula(std::vector<simpleClause> &clauses, unsigned *varCount, int rootRank)
{
    // Bcast varCount
    MPI_Bcast(varCount, 1, MPI_UNSIGNED, rootRank, MPI_COMM_WORLD);

    LOGDEBUG1("VarCount = %u", *varCount);

    u_int64_t bufferSize;
    std::vector<std::vector<char>> serializedChunks;

    if (mpi_rank == rootRank)
    {
        LOGDEBUG1("Root clauses number: %u", clauses.size());
        // Serialize clauses into chunks
        if (!serializeClauses(clauses, serializedChunks))
        {
            LOGERROR("Error serializing clauses");
            return false;
        }

        bufferSize = serializedChunks.size();
    }

    // Broadcast the number of chunks first
    MPI_Bcast(&bufferSize, 1, MPI_UNSIGNED_LONG_LONG, rootRank, MPI_COMM_WORLD);

    LOGDEBUG1("Number of chunks is %lu", bufferSize);

    serializedChunks.resize(bufferSize);
    for (u_int64_t i = 0; i < bufferSize; ++i)
    {
        if (mpi_rank != rootRank)
        {
            // Resize chunk buffer to receive the correct size
            u_int64_t chunkSize;
            MPI_Bcast(&chunkSize, 1, MPI_UNSIGNED_LONG_LONG, rootRank, MPI_COMM_WORLD);
            serializedChunks[i].resize(chunkSize);
        }
        else
        {
            // Send chunk size
            u_int64_t chunkSize = serializedChunks[i].size();
            MPI_Bcast(&chunkSize, 1, MPI_UNSIGNED_LONG_LONG, rootRank, MPI_COMM_WORLD);
        }

        // Broadcast the actual chunk data
        MPI_Bcast(serializedChunks[i].data(), serializedChunks[i].size(), MPI_CHAR, rootRank, MPI_COMM_WORLD);
    }

    if (mpi_rank != rootRank)
    {
        // Deserialize serializedChunks into clauses
        if (!deserializeClauses(serializedChunks, clauses))
        {
            LOGERROR("Error deserializing clauses");
            return false;
        }
        LOGDEBUG1("Worker: deserialized %u clauses", clauses.size());
    }

    return true;
}
