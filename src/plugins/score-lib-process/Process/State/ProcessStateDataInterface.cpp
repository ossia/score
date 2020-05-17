// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ProcessStateDataInterface.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(ProcessStateDataInterface)
ProcessStateDataInterface::ProcessStateDataInterface(Process::ProcessModel& model, QObject* parent)
    : IdentifiedObject{Id<ProcessStateDataInterface>{}, "", parent}, m_model{model}
{
  connect(this, &ProcessStateDataInterface::stateChanged, this, [&]() {
    messagesChanged(this->messages());
  });
}

ProcessStateDataInterface::~ProcessStateDataInterface() = default;
