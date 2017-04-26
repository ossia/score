#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessMetadata.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
namespace RecordedMessages
{
using ProcessFactory
    = Process::GenericProcessModelFactory<RecordedMessages::ProcessModel>;
using LayerFactory
    = Process::GenericDefaultLayerFactory<RecordedMessages::ProcessModel>;
}
