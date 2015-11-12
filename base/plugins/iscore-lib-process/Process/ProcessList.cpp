#include "ProcessList.hpp"

ProcessList& SingletonProcessList::instance()
{
    static ProcessList instance;
    return instance;
}
