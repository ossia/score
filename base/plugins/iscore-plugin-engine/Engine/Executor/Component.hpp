#pragma once
#include <iscore/model/Component.hpp>
#include <iscore_plugin_engine_export.h>
namespace Engine
{
namespace Execution
{
struct Context;
using Component = iscore::GenericComponent<const Context>;
}
}
