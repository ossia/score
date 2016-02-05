#include "ProcessStateDataInterface.hpp"

ProcessStateDataInterface::ProcessStateDataInterface(
        Process::ProcessModel&model,
        QObject *parent):
    IdentifiedObject{Id<ProcessStateDataInterface>{}, "", parent},
    m_model{model}
{
    connect(this, &ProcessStateDataInterface::stateChanged,
            this, [&] () {
        messagesChanged(this->messages());
    });
}

ProcessStateDataInterface::~ProcessStateDataInterface()
{

}
