#include "AutomationExecutor.hpp"
#include "AutomationModel.hpp"

class DeviceTree
{
    public:
        template<typename... Args>
        static void sendMessage(const QString& address, Args&&... args)
        {

            // Get the Device from the beginning of the address
            // Send it the message
        }

        static void check()
        {

        }
};

void AutomationExecutor::start()
{

}


void AutomationExecutor::stop()
{

}


void AutomationExecutor::onTick(const TimeValue& time)
{
    DeviceTree::sendMessage(m_model.address(), m_model.value(time));
}


AutomationExecutor::AutomationExecutor(AutomationModel& model):
    m_model{model}
{

}
