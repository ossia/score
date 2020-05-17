#pragma once
#include <Process/ProcessMetadata.hpp>

#include <score/plugins/UuidKey.hpp>

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
    Process::ProcessCategory::Structure,
    "Structure",
    "Temporal looping structure",
    "ossia score",
    {},
    {},
    {},
    Process::ProcessFlags::SupportsTemporal | Process::ProcessFlags::PutInNewSlot)
UNDO_NAME_METADATA(EMPTY_MACRO, Loop::ProcessModel, "Loop")
