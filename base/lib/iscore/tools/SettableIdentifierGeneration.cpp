#include <cstdint>
#include <limits>
#include <random>

#include "SettableIdentifierGeneration.hpp"

namespace iscore
{
#ifdef ISCORE_VALGRIND_IDS
int32_t random_id_generator::getRandomId()
{
    static int x = 15;
    return x++;
}

#else
int32_t random_id_generator::getRandomId()
{
    using namespace std;
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<int32_t>
    dist(numeric_limits<int32_t>::min(),
         numeric_limits<int32_t>::max());

    return dist(gen);
}

#endif
}

