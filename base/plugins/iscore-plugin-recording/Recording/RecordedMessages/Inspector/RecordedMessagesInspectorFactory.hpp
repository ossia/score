#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Recording/RecordedMessages/Inspector/RecordedMessagesInspectorWidget.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>

namespace RecordedMessages
{
class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
  ISCORE_CONCRETE("cc0c927c-947e-4aed-b2b9-9eab2903c63d")
};
}
