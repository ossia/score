#pragma once
#include <score/model/Component.hpp>
#include <score_plugin_engine_export.h>
namespace Engine
{
namespace Execution
{
struct Context;
using Component = score::GenericComponent<const Context>;
}
}
