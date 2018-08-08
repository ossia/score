#pragma once
#include <score/command/Command.hpp>
#include <score_plugin_automation_export.h>
namespace Automation
{
SCORE_PLUGIN_AUTOMATION_EXPORT
const CommandGroupKey& CommandFactoryName();
}

namespace Gradient
{
using namespace Automation;
}
