#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <RecordedMessages/RecordedMessagesProcessMetadata.hpp>

namespace RecordedMessages
{
using ProcessFactory = Process::GenericDefaultProcessFactory<RecordedMessages::ProcessModel>;
}
