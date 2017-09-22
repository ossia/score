#pragma once
#include <QObject>
#include <score_lib_base_export.h>

namespace score
{
class ApplicationPlugin;
struct ApplicationContext;
struct GUIApplicationContext;
class GUIApplicationPlugin;

class SCORE_LIB_BASE_EXPORT ApplicationPlugin_QtInterface
{
public:
  virtual ~ApplicationPlugin_QtInterface();

  virtual ApplicationPlugin*
  make_applicationPlugin(const score::ApplicationContext& app);
  virtual GUIApplicationPlugin*
  make_guiApplicationPlugin(const score::GUIApplicationContext& app);
};
}

#define ApplicationPlugin_QtInterface_iid \
  "org.ossia.score.plugins.ApplicationPlugin_QtInterface"

Q_DECLARE_INTERFACE(
    score::ApplicationPlugin_QtInterface,
    ApplicationPlugin_QtInterface_iid)
