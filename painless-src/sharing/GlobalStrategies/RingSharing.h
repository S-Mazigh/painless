#ifndef NDIST
#pragma once

#include "GlobalSharingStrategy.h"

/// @brief A strategy for sharing between different mpi processes using a ring topology
/// \ingroup global_sharing
class RingSharing : public GlobalSharingStrategy
{
public:
    /// @brief Constructor
    RingSharing(int id, GlobalDatabase *g_base);

    /// Destructor
    ~RingSharing();

    /// @brief Post constructor initialization: neighbor init
    bool initMpiVariables() override;

    /// @brief Do the sharing following a ring topology
    /// @param g_sharer the global sharer object to access the clauses
    /// @return true if global sharer can break and join, false otherwise
    bool doSharing() override;

    /// @brief test if the serialization and deserialization don't modify the data
    /// @return if the test run correctly
    bool testIntegrity();

protected:
    /// @brief A function that serializes ClauseExchange from clausesToSend database to an int vector
    /// @param serialized_v_cls the vector that will hold the serialization (it is an append operation !!)
    /// @return the number of clauses serialized
    int serializeClauses(std::vector<int> &serialized_v_cls);

    /// @brief A function that deserialize a buffer of int to ClauseExchange object and adds them to the receivedClause and clausesToSend databases (no bloom version)
    /// @param serialized_v_cls the vector with the serialized data
    /// @param add defines the method to be called to add the deserialized clause to a database
    void deserializeClauses(std::vector<int> &serialized_v_cls);

    /// @brief the size of the buffer to send
    uint totalSize;

    /// @brief bloom filter at received
    /// @details two bloom filters are used to not block the propagation.
    BloomFilter * b_filter_received;

    /// @brief bloom filter at send
    BloomFilter * b_filter_send;

    /// @brief Bool to know if request to end was sent to the neighbors
    bool requests_sent;

    /// @brief Request of the non blocking receive end from left neighbor
    MPI_Request end_request_left;

    /// @brief Request of the non blocking receive end from right neighbor
    MPI_Request end_request_right;

    /// @brief Request of the sent messages to my left neighbor
    MPI_Request send_request_left;

    /// @brief Request of the sent messages to my right neighbor
    MPI_Request send_request_right;

    /// @brief Stores the result received from others
    SatResult receivedFinalResult;

    /// @brief Buffer of the clauses to send to my left neighbors
    std::vector<int> clausesToSendSerializedLeft;

    /// @brief Buffer of the clauses to send to my right neighbors
    std::vector<int> clausesToSendSerializedRight;

    /// @brief A tmp buffer since we cannot use the clausesToSend buffers freely because of the MPI_Isend
    std::vector<int> tmp_serializedClauses;

    /// @brief Buffer of the clauses received from a neighbor
    std::vector<int> receivedClauses;

    /// @brief left nighbor mpi_world index
    int left_neighbor;

    /// @brief left nighbor mpi_world index
    int right_neighbor;
};
#endif