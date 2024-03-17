#include "ClauseExchange.h"

inline void printClauseExchange(ClauseExchange *cls)
{
    cout << "size: " << cls->size;
    cout << " lbd: " << cls->lbd;
    cout << " from: " << cls->from;
    cout << " {";

    if (cls->size > 0)
    {
        cout << cls->lits[0];
    }

    for (int i = 1; i < cls->size; i++)
    {
        cout << ", " << cls->lits[i];
    }

    cout << "}" << endl;
}

// Not sure it works with int32_t, the original uses uint32_t
std::size_t hash_vector(std::vector<int32_t> const &vec)
{
    std::size_t seed = vec.size();
    for (auto x : vec)
    {
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = ((x >> 16) ^ x) * 0x45d9f3b;
        x = (x >> 16) ^ x;
        seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    return seed;
}

bool operator==(const std::vector<int> &vector1, const std::vector<int> &vector2)
{
    int size1 = vector1.size(), size2 = vector2.size();
    if (size1 != size2)
        return false;

    bool in;
    for (int i = 0; i < size1; i++)
    {
        in = false;
        for (int j = 0; j < size2; j++)
        {
            if (vector2[j] == vector1[i])
            {
                in = true;
                break;
            }
        }
        if (!in)
            return false;
    }
    return true;

    // return hash_vector(vector1) == hash_vector(vector2);
}

// // compare only size and lbd to speed up the comparison
// bool operator==(const ClauseExchange &cls1, const ClauseExchange &cls2)
// {
//     return (cls1.lbd == cls2.lbd && cls1.size == cls2.size);
// }

// bool operator!=(const ClauseExchange &cls1, const ClauseExchange &cls2)
// {
//     return !operator==(cls1, cls2);
// }

// bool operator<(const ClauseExchange &cls1, const ClauseExchange &cls2)
// {
//     return (cls1.size < cls2.size || cls1.lbd < cls2.lbd);
// }

// bool operator>(const ClauseExchange &cls1, const ClauseExchange &cls2)
// {
//     return operator<(cls2, cls1);
// }

// bool operator<=(const ClauseExchange &cls1, const ClauseExchange &cls2)
// {
//     return !(cls1 < cls2);
// }

// bool operator>=(const ClauseExchange &cls1, const ClauseExchange &cls2)
// {
//     return !operator>(cls1, cls2);
// }