#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/LayerModel.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <Recording/RecordedMessages/RecordedMessagesProcessMetadata.hpp>

namespace RecordedMessages
{
using Layer = Process::LayerModel_T<RecordedMessages::ProcessModel>;
}

LAYER_METADATA(
        ,
        RecordedMessages::Layer,
        "a82bf020-7752-440d-99e7-395673450586",
        "MessagesLayer",
        "MessagesLayer"
        )

namespace RecordedMessages
{
using ProcessFactory = Process::GenericProcessModelFactory<RecordedMessages::ProcessModel>;
using LayerFactory = Process::GenericDefaultLayerFactory<RecordedMessages::Layer>;
}
