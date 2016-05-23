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

}

ISCORE_LIB_BASE_EXPORT const iscore::ApplicationContext& iscore::AppContext()
{
    return ApplicationInterface::instance().context();
}
