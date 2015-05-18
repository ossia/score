#include "AutomationExecutor.hpp"

#include <Automation/AutomationModel.hpp>
#include <Plugin/DeviceExplorerPlugin.hpp>

void AutomationExecutor::start()
{

}


void AutomationExecutor::stop()
{

}

void AutomationExecutor::onTick(const TimeValue& time)
{
    /*
    SingletonDeviceList::sendMessage(m_model.address(),
                                     m_model.value(time));
                                     */
}

AutomationExecutor::AutomationExecutor(AutomationModel& model):
    m_model{model}
{

}
