#pragma once
#include <QObject>
#include <iscore_lib_base_export.h>

namespace iscore
{
class ApplicationPlugin;
struct ApplicationContext;
struct GUIApplicationContext;
class GUIApplicationPlugin;

class ISCORE_LIB_BASE_EXPORT ApplicationPlugin_QtInterface
{
public:
  virtual ~ApplicationPlugin_QtInterface();

  virtual ApplicationPlugin*
  make_applicationPlugin(const iscore::ApplicationContext& app);
  virtual GUIApplicationPlugin*
  make_guiApplicationPlugin(const iscore::GUIApplicationContext& app);
};
}

#define ApplicationPlugin_QtInterface_iid \
  "org.ossia.i-score.plugins.ApplicationPlugin_QtInterface"

Q_DECLARE_INTERFACE(
    iscore::ApplicationPlugin_QtInterface,
    ApplicationPlugin_QtInterface_iid)
