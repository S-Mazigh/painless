
#include "sharing/SharingEntity.h"

SharingEntity::~SharingEntity()
{
}

SharingEntity::SharingEntity(int _id)
{
    id = _id;
}

/// Decrease the counter of references of this solver, delete it if needed.
void SharingEntity::release()
{
    int oldValue = nRefs.fetch_sub(1);

    // #ifndef QUIET
    //     LOG(4, "Entity %d : %d -> %d\n", this->id, oldValue, oldValue - 1);
    // #endif

    if (oldValue - 1 == 0)
    {
        delete this;
    }
}

void SharingEntity::increase()
{
    // #ifndef QUIET
    //     LOG(4, "Entity %d : %d -> %d\n", this->id, nRefs.load(), nRefs.load() + 1);
    // #endif
    nRefs++;
}
