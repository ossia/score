#include <JS/Qml/ViewContext.hpp>

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/IntervalPresenter.hpp>
#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/SlotPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <Scenario/Document/ScenarioDocument/SnapshotAction.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentView.hpp>

#include <QQmlEngine>
#include <QApplication>
#include <QGuiApplication>
#include <QMainWindow>
#include <QPixmap>
#include <QScreen>
#include <QWidget>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::JsViewContext)

namespace JS
{

const score::DocumentContext* JsViewContext::ctx()
{
  return score::GUIAppContext().currentDocument();
}

Scenario::ScenarioDocumentView* JsViewContext::view()
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  // No GUI (e.g. --no-gui): there is no view.
  auto dv = doc->document.view();
  if(!dv)
    return nullptr;
  return qobject_cast<Scenario::ScenarioDocumentView*>(&dv->viewDelegate());
}

Scenario::ScenarioDocumentPresenter* JsViewContext::pres()
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  // Returns nullptr when there is no presenter (e.g. --no-gui).
  return score::IDocument::try_presenterDelegate<Scenario::ScenarioDocumentPresenter>(
      doc->document);
}

bool JsViewContext::grabScene(QString path)
{
  auto v = view();
  if(!v)
    return false;
  return !Scenario::renderSceneToSvg(v->scene(), path).isEmpty();
}

bool JsViewContext::grabMainWindow(QString path)
{
  // The active window may be an output window opened by a device
  QWidget* w = score::GUIAppContext().mainWindow;
  if(!w)
    w = qApp->activeWindow();
  if(!w)
    return false;
  return w->grab().save(path);
}

bool JsViewContext::grabWidget(QObject* widget, QString path)
{
  auto* w = qobject_cast<QWidget*>(widget);
  if(!w)
    return false;
  return w->grab().save(path);
}

namespace
{
//! The engine takes ownership of a QObject it is handed unless told not to,
//! and a panel widget is parented late enough to look collectable. Without
//! this, destroy() on one of these deletes the live panel.
QObject* keep(QObject* o)
{
  if(o)
    QQmlEngine::setObjectOwnership(o, QQmlEngine::CppOwnership);
  return o;
}
}

QObject* JsViewContext::panel(QString name)
{
  // The pretty name is what the header shows, and it is translated; the
  // widget's class name is not, so a script keeps working under a translated
  // UI. panels() lists both.
  for(auto& p : score::GUIAppContext().panels())
  {
    if(p.defaultPanelStatus().prettyName.compare(name, Qt::CaseInsensitive) == 0)
      return keep(p.widget());
  }

  for(auto& p : score::GUIAppContext().panels())
  {
    auto* w = p.widget();
    if(w
       && name.compare(
              QString::fromUtf8(w->metaObject()->className()), Qt::CaseInsensitive)
              == 0)
      return keep(w);
  }
  return nullptr;
}

QStringList JsViewContext::panels()
{
  QStringList out;
  for(auto& p : score::GUIAppContext().panels())
  {
    out += p.defaultPanelStatus().prettyName;
    if(auto* w = p.widget())
      out += QString::fromUtf8(w->metaObject()->className());
  }
  return out;
}

QObject* JsViewContext::child(QObject* parent, QString className)
{
  if(!parent)
    return nullptr;

  // By class name: the widgets inside a panel are rarely named.
  const auto utf8 = className.toUtf8();
  for(auto* o : parent->findChildren<QObject*>())
    if(o->inherits(utf8.constData()))
      return keep(o);
  return nullptr;
}

bool JsViewContext::grabScreen(QString path)
{
  auto screen = QGuiApplication::primaryScreen();
  if(!screen)
    return false;
  return screen->grabWindow(0).save(path);
}

void JsViewContext::zoom(double zx, double zy)
{
  if(auto v = view())
    v->zoom(zx, zy);
}

void JsViewContext::scroll(double dx, double dy)
{
  if(auto v = view())
    v->scroll(dx, dy);
}

void JsViewContext::setZoomRatio(double r)
{
  if(auto p = pres())
    p->setZoomRatio(r);
}

void JsViewContext::centerOn(QObject* process)
{
  auto p = pres();
  auto v = view();
  if(!p || !v)
    return;
  auto* model = qobject_cast<Process::ProcessModel*>(process);
  if(!model)
    return;
  const auto& target = model->id();

  QGraphicsItem* found = nullptr;

  // Nodal (dataflow) mode: the process is drawn as a NodeItem in the scene.
  for(auto* it : v->scene().items())
  {
    if(auto* node = dynamic_cast<Process::NodeItem*>(it))
    {
      if(node->id() == target)
      {
        found = node;
        break;
      }
    }
  }

  // Temporal mode: walk the displayed interval's slots/layers.
  if(!found)
  {
    if(auto* itv = p->displayedIntervalPresenter())
    {
      for(const auto& slot : itv->getSlots())
      {
        if(auto* ls = slot.getLayerSlot())
        {
          for(const auto& ld : ls->layers)
          {
            if(ld.model().id() == target && !ld.layers().empty())
            {
              found = ld.layers().front().container;
              break;
            }
          }
        }
        if(found)
          break;
      }
    }
  }

  if(found)
    v->view().centerOn(found);
}

void JsViewContext::goToInterval(QObject* interval)
{
  auto p = pres();
  if(!p)
    return;
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(interval))
    p->setDisplayedInterval(itv);
}

void JsViewContext::fit()
{
  if(auto p = pres())
    p->setLargeView();
}

void JsViewContext::recenter()
{
  if(auto p = pres())
    p->recenter();
}

void JsViewContext::setNodal(bool nodal)
{
  if(auto p = pres())
    p->setNodalMode(nodal);
}

bool JsViewContext::isNodal()
{
  if(auto p = pres())
    return p->isNodal();
  return false;
}
}
