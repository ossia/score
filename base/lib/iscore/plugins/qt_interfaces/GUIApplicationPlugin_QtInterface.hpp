#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>

namespace iscore
{
struct GUIApplicationContext;
class GUIApplicationPlugin;

class ISCORE_LIB_BASE_EXPORT GUIApplicationPlugin_QtInterface
{
public:
  virtual ~GUIApplicationPlugin_QtInterface();

  virtual GUIApplicationPlugin*
  make_applicationPlugin(const iscore::GUIApplicationContext& app)
      = 0;
};
}

#define GUIApplicationPlugin_QtInterface_iid \
  "org.ossia.i-score.plugins.GUIApplicationPlugin_QtInterface"

Q_DECLARE_INTERFACE(
    iscore::GUIApplicationPlugin_QtInterface,
    GUIApplicationPlugin_QtInterface_iid)
