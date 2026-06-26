#pragma once
#include <score_plugin_js_export.h>

#include <QObject>
#include <QString>

#include <verdigris>

namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class ScenarioDocumentView;
class ScenarioDocumentPresenter;
}

namespace JS
{
//! JS object exposed as `View`: automation of the main scenario view.
//!
//! Every method degrades gracefully when there is no GUI (`--no-gui`): the
//! view/presenter accessors return nullptr and the methods become no-ops.
class SCORE_PLUGIN_JS_EXPORT JsViewContext : public QObject
{
  W_OBJECT(JsViewContext)
public:
  // Screenshots
  bool grabScene(QString path);
  W_SLOT(grabScene)
  bool grabMainWindow(QString path);
  W_SLOT(grabMainWindow)
  bool grabScreen(QString path);
  W_SLOT(grabScreen)

  // Zoom / scroll
  void zoom(double zx, double zy);
  W_SLOT(zoom)
  void scroll(double dx, double dy);
  W_SLOT(scroll)
  void setZoomRatio(double r);
  W_SLOT(setZoomRatio)

  // Navigation / focus
  void centerOn(QObject* process);
  W_SLOT(centerOn)
  void goToInterval(QObject* interval);
  W_SLOT(goToInterval)
  void fit();
  W_SLOT(fit)
  void recenter();
  W_SLOT(recenter)

  // Dataflow / temporal mode
  void setNodal(bool nodal);
  W_SLOT(setNodal)
  bool isNodal();
  W_SLOT(isNodal)

private:
  const score::DocumentContext* ctx();
  Scenario::ScenarioDocumentView* view();
  Scenario::ScenarioDocumentPresenter* pres();
};
}
