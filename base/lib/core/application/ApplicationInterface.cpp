#include "ApplicationInterface.hpp"
#include <iscore/application/ApplicationContext.hpp>
namespace iscore
{
ApplicationInterface* ApplicationInterface::m_instance;
ApplicationInterface::~ApplicationInterface() = default;

ApplicationInterface& ApplicationInterface::instance()
{
    return *m_instance;
}


ISCORE_LIB_BASE_EXPORT const ApplicationContext& AppContext()
{
    return ApplicationInterface::instance().context();
}

ISCORE_LIB_BASE_EXPORT const ApplicationComponents& AppComponents()
{
    return ApplicationInterface::instance().components();
}

}
