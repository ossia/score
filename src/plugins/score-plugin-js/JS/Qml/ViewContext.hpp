#pragma once
#include <score_plugin_js_export.h>

#include <QObject>
#include <QString>
#include <QStringList>

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

  //! Any widget, not just the main window. QWidget::grab() is not invokable,
  //! so it cannot be called from JS on a widget from panel() / child().
  bool grabWidget(QObject* widget, QString path);
  W_SLOT(grabWidget)

  // Reaching the rest of the UI. Anything declared a slot on a QObject is
  // already callable from QML; what was missing was a way to get at the
  // objects. expandAll, setCurrentIndex, resize and the rest are Qt's own.

  //! A dock panel's widget by the name it shows, e.g. "Device Explorer".
  //! Case-insensitive; null when there is no such panel.
  QObject* panel(QString name);
  W_SLOT(panel)

  //! The first descendant of the given class, e.g. child(p, "QTreeView").
  //! Null when `parent` is null or nothing matches.
  QObject* child(QObject* parent, QString className);
  W_SLOT(child)

  //! The names of the panels that exist: the translated one the header shows,
  //! and the widget class name, which panel() also accepts and which does not
  //! move with the language.
  QStringList panels();
  W_SLOT(panels)

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
