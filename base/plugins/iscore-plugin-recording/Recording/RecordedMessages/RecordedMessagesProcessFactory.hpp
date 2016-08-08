#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessMetadata.hpp>

namespace RecordedMessages
{
using ProcessFactory = Process::GenericDefaultProcessFactory<RecordedMessages::ProcessModel>;
}
