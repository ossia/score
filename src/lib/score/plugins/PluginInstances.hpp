#pragma once
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/tools/Debug.hpp>

#include <boost/config.hpp>

#include <score_lib_base_export.h>

#include <vector>
namespace score
{
SCORE_LIB_BASE_EXPORT
std::vector<Plugin_QtInterface*>& staticPlugins();
}

#if defined(SCORE_JIT_ID)

#define SCORE_JIT_PASTER(x, y) x##_##y
#define SCORE_JIT_EVALUATOR(x, y) SCORE_JIT_PASTER(x, y)
#define SCORE_JIT_NAME(fun) SCORE_JIT_EVALUATOR(fun, SCORE_JIT_ID)

#define SCORE_EXPORT_PLUGIN(classname)                                                  \
  extern "C" Q_DECL_EXPORT score::Plugin_QtInterface* SCORE_JIT_NAME(plugin_instance)() \
  {                                                                                     \
    static classname p;                                                                 \
    return &p;                                                                          \
  }

#elif defined(SCORE_STATIC_PLUGINS)
#define SCORE_EXPORT_PLUGIN(classname)

#elif defined(SCORE_DYNAMIC_PLUGINS)
#define SCORE_EXPORT_PLUGIN(classname)                                        \
  extern "C" BOOST_SYMBOL_EXPORT score::Plugin_QtInterface* plugin_instance() \
  {                                                                           \
    static classname p;                                                       \
    return &p;                                                                \
  }

#else
#define SCORE_EXPORT_PLUGIN(classname)                                  \
  extern "C" Q_DECL_EXPORT score::Plugin_QtInterface* plugin_instance() \
  {                                                                     \
    static classname p;                                                 \
    return &p;                                                          \
  }
#endif
