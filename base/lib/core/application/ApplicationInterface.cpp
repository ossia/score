#include "ApplicationInterface.hpp"
#include <iscore/application/ApplicationContext.hpp>
namespace iscore
{
ApplicationInterface* ApplicationInterface::m_instance;
ApplicationInterface::~ApplicationInterface()
{

}

ApplicationInterface& ApplicationInterface::instance()
{
    return *m_instance;
}

const ApplicationContext& AppContext()
{
    return ApplicationInterface::instance().context();
}
}
