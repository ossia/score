#include "SettableIdentifierGeneration.hpp"

#include <random>

int32_t getNextId()
{
    static int x = 15;
    return x++;
    /*
    using namespace std;
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_int_distribution<int32_t>
    dist(numeric_limits<int32_t>::min(),
         numeric_limits<int32_t>::max());

    return dist(gen);
    */
}
