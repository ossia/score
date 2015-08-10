#include "SingletonAreaFactoryList.hpp"

AreaFactoryList& SingletonAreaFactoryList::instance()
{
    static AreaFactoryList instance;
    return instance;
}
