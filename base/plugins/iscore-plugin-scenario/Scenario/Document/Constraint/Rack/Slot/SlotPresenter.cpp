#include <Process/LayerModel.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackView.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotView.hpp>
#include <iscore/tools/std/Optional.hpp>

#include "SlotHandle.hpp"
#include "SlotPresenter.hpp"
#include <Process/ProcessContext.hpp>
#include <Process/TimeValue.hpp>
#include <Process/ZoomHelper.hpp>
#include <QMenu>
#include <Scenario/Application/Menus/ScenarioContextMenuManager.hpp>
#include <algorithm>
#include <boost/range/algorithm_ext/erase.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/widgets/GraphicsItem.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/Todo.hpp>

class QObject;
namespace Scenario
{
SlotPresenter::SlotPresenter(
    const SlotModel& model,
    RackView* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* par)
    : QObject{par}
    , m_processList{ctx.app.interfaces<Process::LayerFactoryList>()}
    , m_model{model}
    , m_view{new SlotView{*this, view}}
    , m_context{ctx}
{
  m_view->setPosition(QPointF(0, 0));

  for (const auto& proc_vm : m_model.layers)
  {
    on_layerModelCreated_impl(proc_vm);
  }

  m_model.layers.added
      .connect<SlotPresenter, &SlotPresenter::on_layerModelCreated>(this);
  m_model.layers.removed
      .connect<SlotPresenter, &SlotPresenter::on_layerModelRemoved>(this);

  con(m_model, &SlotModel::layerModelPutToFront, this,
      &SlotPresenter::on_layerModelPutToFront);

  con(m_model, &SlotModel::HeightChanged, this,
      &SlotPresenter::on_heightChanged);

  con(m_model, &SlotModel::focusChanged, m_view, &SlotView::setFocus);
  m_view->setHeight(m_model.getHeight());

  connect(
      m_view, &SlotView::askContextMenu, this,
      [&](const QPoint& pos, const QPointF& scenept) {
        QMenu* menu = new QMenu;
        ScenarioContextMenuManager::createSlotContextMenu(ctx, *menu, *this);
        menu->exec(pos);
        menu->close();
        menu->deleteLater();
      });

  if (auto frontLayer = m_model.frontLayerModel())
    on_layerModelPutToFront(*frontLayer);
}

SlotPresenter::~SlotPresenter()
{
  for (auto& proc : m_processes)
    delete proc.presenter;
}

const Id<SlotModel>& SlotPresenter::id() const
{
  return m_model.id();
}

const SlotModel& SlotPresenter::model() const
{
  return m_model;
}

int SlotPresenter::height() const
{
  return m_view->height();
}

void SlotPresenter::setWidth(qreal w)
{
  m_view->setWidth(w);

  updateProcesses();
}

void SlotPresenter::setVerticalPosition(double pos)
{
  auto view_pos = m_view->position();

  if (view_pos.y() != pos)
  {
    m_view->setPosition(QPointF(view_pos.x(), pos));
    m_view->update();
  }
}

void SlotPresenter::enable()
{
  m_view->enable();
  for (auto& elt : m_processes)
  {
    elt.presenter->parentGeometryChanged();
  }

  if (auto frontLayer = m_model.frontLayerModel())
    on_layerModelPutToFront(*frontLayer);

  m_enabled = true;
}

void SlotPresenter::disable()
{
  m_view->disable();

  for (auto& elt : m_processes)
  {
    elt.presenter->parentGeometryChanged();
    elt.presenter->putBehind();
  }

  m_enabled = false;
}

void SlotPresenter::on_layerModelCreated(const Process::LayerModel& layerModel)
{
  on_layerModelCreated_impl(layerModel);
}

void SlotPresenter::on_layerModelRemoved(const Process::LayerModel& layerModel)
{
  boost::remove_erase_if(m_processes, [&](auto& elt) {
    bool to_delete = elt.model->id() == layerModel.id();

    if (to_delete)
    {
      // No need to delete the view, the process presenters already do it.
      QPointer<Process::LayerView> view_p{elt.view};
      delete elt.presenter;
      if (view_p)
        deleteGraphicsItem(elt.view);
    }

    return to_delete;
  });

  updateProcessesShape();
  emit askUpdate();
}

void SlotPresenter::on_layerModelPutToFront(const Process::LayerModel& layer)
{
  // Put the selected one at z+1 and the others at -z; set "disabled" graphics
  // mode.
  // OPTIMIZEME by saving the previous to front and just switching...
  for (auto& elt : m_processes)
  {
    if (elt.model->id() == layer.id())
    {
      elt.presenter->putToFront();
    }
    else
    {
      elt.presenter->putBehind();
    }
  }
}

void SlotPresenter::on_heightChanged(double height)
{
  m_view->setHeight(height);
  updateProcessesShape();

  emit askUpdate();
}

void SlotPresenter::on_parentGeometryChanged()
{
  updateProcessesShape();
}

void SlotPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;

  for (auto& elt : m_processes)
  {
    elt.presenter->on_zoomRatioChanged(m_zoomRatio);
  }

  updateProcessesShape();
}

void SlotPresenter::on_layerModelCreated_impl(
    const Process::LayerModel& proc_vm)
{
  const auto& procKey = proc_vm.processModel().concreteKey();

  auto factory = m_processList.findDefaultFactory(procKey);
  ISCORE_ASSERT(factory);

  auto proc_view = factory->makeLayerView(proc_vm, m_view);
  auto proc_pres
      = factory->makeLayerPresenter(proc_vm, proc_view, m_context, this);

  m_processes.emplace_back(&proc_vm, proc_pres, proc_view);

  auto con_id = con(
      proc_vm.processModel(), &Process::ProcessModel::durationChanged, this,
      [&](const TimeVal&) {
        // TODO index instead
        auto it = std::find_if(
            m_processes.begin(), m_processes.end(), [&](const auto& elt) {
              return elt.model->processModel().id()
                     == proc_vm.processModel().id();
            });

        // TODO this should be an assert but it sometimes causes crashes.
        if (it != m_processes.end())
          updateProcessShape(*it);
      });
  con(proc_vm, &IdentifiedObjectAbstract::identified_object_destroying, this,
      [=] { QObject::disconnect(con_id); });

  if (m_enabled)
    m_view->enable();
  else
    m_view->disable();

  auto frontLayer = m_model.frontLayerModel();
  if (frontLayer && (frontLayer->id() == proc_vm.id()))
  {
    on_layerModelPutToFront(proc_vm);
  }

  proc_pres->on_zoomRatioChanged(m_zoomRatio);

  updateProcessesShape();
}

void SlotPresenter::updateProcesses()
{
  updateProcessesShape();
}

void SlotPresenter::updateProcessShape(const SlotProcessData& data)
{
  data.presenter->setHeight(height() - SlotHandle::handleHeight());

  auto width = data.model->processModel().duration().toPixels(m_zoomRatio);
  data.presenter->setWidth(width);
  data.presenter->parentGeometryChanged();
}

void SlotPresenter::updateProcessesShape()
{
  for (auto& elt : m_processes)
  {
    updateProcessShape(elt);
  }
}
}
