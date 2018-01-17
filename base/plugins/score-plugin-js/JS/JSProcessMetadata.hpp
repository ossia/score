#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <score_plugin_js_export.h>

namespace JS
{
class ProcessModel;
}

PROCESS_METADATA(
    SCORE_PLUGIN_JS_EXPORT,
    JS::ProcessModel,
    "846a5de5-47f9-46c5-a898-013cb20951d0",
    "Javascript",
    "Javascript",
    "Script",
    {},
    Process::ProcessFlags::SupportsAll |
    Process::ProcessFlags::PutInNewSlot)
