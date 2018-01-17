#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score_plugin_loop_export.h>

namespace Loop
{
class ProcessModel;
class Layer;
}

PROCESS_METADATA(
    SCORE_PLUGIN_LOOP_EXPORT,
    Loop::ProcessModel,
    "995d41a8-0f10-4152-971d-e4c033579a02",
    "Loop",
    "Loop",
    "Structure",
    {},
    Process::ProcessFlags::SupportsTemporal |
    Process::ProcessFlags::PutInNewSlot)
UNDO_NAME_METADATA(EMPTY_MACRO, Loop::ProcessModel, "Loop")
