#pragma once
#include <score/tools/Todo.hpp>
#include <vector>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score_lib_base_export.h>

namespace score
{
SCORE_LIB_BASE_EXPORT
std::vector<Plugin_QtInterface*>& staticPlugins();
}

#if defined(SCORE_STATIC_PLUGINS)
#define SCORE_EXPORT_PLUGIN(classname)
#else
#define SCORE_EXPORT_PLUGIN(classname) \
extern "C" Q_DECL_EXPORT score::Plugin_QtInterface* plugin_instance() { static classname p; return &p; }
#endif
