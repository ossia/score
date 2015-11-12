#include "SingletonProtocolList.hpp"

ProtocolList& SingletonProtocolList::instance()
{
    static ProtocolList instance;
    return instance;
}
