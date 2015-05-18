#include "SingletonProtocolList.hpp"
ProtocolList& SingletonProtocolList::instance()
{
    static ProtocolList m_instance;
    return m_instance;
}
