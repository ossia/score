#pragma once
#include <Process/ProcessMetadata.hpp>
#include <QString>
#include <iscore_plugin_js_export.h>

namespace JS
{
class StateProcess;
class ProcessModel;
}

PROCESS_METADATA(
        ISCORE_PLUGIN_JS_EXPORT,
        JS::ProcessModel,
        "846a5de5-47f9-46c5-a898-013cb20951d0",
        "Javascript",
        "Javascript"
        )


STATEPROCESS_METADATA(
        ISCORE_PLUGIN_JS_EXPORT,
        JS::StateProcess,
        "39ab5ce1-27da-41cf-8e8e-de8175d1689f",
        "JSState",
        "Javascript State"
        )
