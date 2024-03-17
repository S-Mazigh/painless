#include "Sharer.h"

static void *mainThrSharingMulti(void *arg);

/// \ingroup sharing
/// @brief A sharer multi is a thread responsible to share clauses between solvers using multiple strategies that will be called one at a time.
/// @details In the current implementation a vector is used to store all the strategies and another to manage the strategies during sharing, since a strategy can stop while the others continue. A circular would be the best performance wise, but since the number of the strategies is rather small, it is put on hold for now.
class SharerMulti : public Sharer
{
public:
    /// @brief Constructor
    /// @param _id the id of this sharer
    /// @param sharingStrategies A vector of sharing strategies that will alternate
    SharerMulti(int id_, std::vector<SharingStrategy *> &sharingStrategies);

    /// Destructor.
    ~SharerMulti();

    /// @brief Prints the stats of the different strategies used by this sharer
    void printStats();

protected:
    friend void *mainThrSharingMulti(void *);

    /// @brief A vector holding the different strategies that this sharer will alternate on
    std::vector<SharingStrategy *> sharingStrategies;
};