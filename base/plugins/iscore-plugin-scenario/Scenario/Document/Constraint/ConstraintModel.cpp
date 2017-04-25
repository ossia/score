#include <Process/LayerModel.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <map>
#include <utility>

#include "ConstraintModel.hpp"
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModel.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/tools/Todo.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
namespace Scenario
{
class StateModel;
class TimeNodeModel;
ConstraintModel::ConstraintModel(
    const Id<ConstraintModel>& id,
    double yPos,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, ConstraintModel>::get(), parent}
{
  initConnections();
  metadata().setInstanceName(*this);
  metadata().setColor(ScenarioStyle::instance().ConstraintDefaultBackground);
  setHeightPercentage(yPos);
}

ConstraintModel::~ConstraintModel()
{
  static_assert(std::is_same<serialization_tag<ConstraintModel>::type, visitor_entity_tag>::value, "");
}
void ConstraintModel::initConnections()
{
  processes.added
      .connect<ConstraintModel, &ConstraintModel::on_addProcess>(this);
  processes.removed
      .connect<ConstraintModel, &ConstraintModel::on_removeProcess>(this);
}

ConstraintModel::ConstraintModel(
    const ConstraintModel& source,
    const Id<ConstraintModel>& id,
    QObject* parent)
    : Entity{source, id, Metadata<ObjectKey_k, ConstraintModel>::get(), parent}
{
  initConnections();
  metadata().setInstanceName(*this);
  // It is not necessary to save modelconsistency because it should be
  // recomputed

  m_smallView = source.m_smallView;
  m_fullView = source.m_fullView;
  m_startState = source.startState();
  m_endState = source.endState();
  duration = source.duration;

  m_startDate = source.m_startDate;
  m_heightPercentage = source.heightPercentage();
  m_zoom = source.m_zoom;
  m_center = source.m_center;
  m_smallViewShown = source.m_smallViewShown;

  // Clone the processes
  for (const auto& process : source.processes)
  {
    auto newproc = process.clone(process.id(), this);
    processes.add(newproc);
    // We don't need to resize them since the new constraint will have the same
    // duration.
  }
}

ConstraintModel::ConstraintModel(
    DataStream::Deserializer& vis,
    QObject* parent) : Entity{vis, parent}
{
  initConnections();
  vis.writeTo(*this);
}

ConstraintModel::ConstraintModel(
    JSONObject::Deserializer& vis,
    QObject* parent) : Entity{vis, parent}
{
  initConnections();
  vis.writeTo(*this);
}

ConstraintModel::ConstraintModel(
    DataStream::Deserializer&& vis,
    QObject* parent) : Entity{vis, parent}
{
  initConnections();
  vis.writeTo(*this);
}

ConstraintModel::ConstraintModel(
    JSONObject::Deserializer&& vis,
    QObject* parent) : Entity{vis, parent}
{
  initConnections();
  vis.writeTo(*this);
}

/* TODO
void ConstraintModel::on_rackAdded(const RackModel& rack)
{
  con(duration, &ConstraintDurations::defaultDurationChanged, &rack,
      &RackModel::on_durationChanged);
}
*/

const Id<StateModel>& ConstraintModel::startState() const
{
  return m_startState;
}

void ConstraintModel::setStartState(const Id<StateModel>& e)
{
  m_startState = e;
}

const Id<StateModel>& ConstraintModel::endState() const
{
  return m_endState;
}

void ConstraintModel::setEndState(const Id<StateModel>& endState)
{
  m_endState = endState;
}

const TimeVal& ConstraintModel::startDate() const
{
  return m_startDate;
}

void ConstraintModel::setStartDate(const TimeVal& start)
{
  m_startDate = start;
  emit startDateChanged(start);
}

void ConstraintModel::translate(const TimeVal& deltaTime)
{
  setStartDate(m_startDate + deltaTime);
}

// Simple getters and setters

double ConstraintModel::heightPercentage() const
{
  return m_heightPercentage;
}

// Should go in an "execution" object.
void ConstraintModel::startExecution()
{
  for (Process::ProcessModel& proc : processes)
  {
    proc.startExecution(); // prevents editing
  }
}
void ConstraintModel::stopExecution()
{
  duration.setPlayPercentage(0);
  duration.setExecutionSpeed(1.0);
  for (Process::ProcessModel& proc : processes)
  {
    proc.stopExecution();
  }
}

void ConstraintModel::reset()
{
  duration.setPlayPercentage(0);
  duration.setExecutionSpeed(1.0);

  for (Process::ProcessModel& proc : processes)
  {
    proc.reset();
    proc.stopExecution();
  }

  setExecutionState(ConstraintExecutionState::Enabled);
}

void ConstraintModel::setHeightPercentage(double arg)
{
  if (m_heightPercentage != arg)
  {
    m_heightPercentage = arg;
    emit heightPercentageChanged(arg);
  }
}

void ConstraintModel::setExecutionState(ConstraintExecutionState s)
{
  if (s != m_executionState)
  {
    m_executionState = s;
    emit executionStateChanged(s);
  }
}

ZoomRatio ConstraintModel::zoom() const
{
  return m_zoom;
}

void ConstraintModel::setZoom(const ZoomRatio& zoom)
{
  m_zoom = zoom;
}

QRectF ConstraintModel::visibleRect() const
{
  return m_center;
}

void ConstraintModel::setVisibleRect(const QRectF& value)
{
  m_center = value;
}

void ConstraintModel::setSmallViewVisible(bool v)
{
  m_smallViewShown = v;
  emit smallViewVisibleChanged(v);
}

bool ConstraintModel::smallViewVisible() const
{
  return m_smallViewShown;
}

void ConstraintModel::clearSmallView()
{
  m_smallView.clear();
  emit rackChanged(Slot::SmallView);
}

void ConstraintModel::clearFullView()
{
  m_fullView.clear();
  emit rackChanged(Slot::FullView);
}

void ConstraintModel::replaceSmallView(const Rack& other)
{
  m_smallView = other;
  emit rackChanged(Slot::SmallView);
}

void ConstraintModel::replaceFullView(const Rack& other)
{
  m_fullView = other;
  emit rackChanged(Slot::FullView);
}

void ConstraintModel::addLayer(const SlotId& slot, Id<Process::ProcessModel> id)
{
  auto& vec = slot.full_view ? m_fullView : m_smallView;

  auto& procs = vec.at(slot.index).processes;
  ISCORE_ASSERT(ossia::find(procs, id) == procs.end());
  procs.push_back(id);

  emit layerAdded(slot, id);

  putLayerToFront(slot, id);
}

void ConstraintModel::removeLayer(const SlotId& slot, Id<Process::ProcessModel> id)
{
  auto& vec = slot.full_view ? m_fullView : m_smallView;

  auto& procs = vec.at(slot.index).processes;
  const auto N = procs.size();
  boost::remove_erase(procs, id);

  if(procs.size() < N)
  {
    emit layerRemoved(slot, id);

    if(!procs.empty())
      putLayerToFront(slot, procs.front());
    else
      putLayerToFront(slot, ossia::none);
  }
}

void ConstraintModel::putLayerToFront(const SlotId& slot, Id<Process::ProcessModel> id)
{
  auto& vec = slot.full_view ? m_fullView : m_smallView;

  vec.at(slot.index).frontProcess = id;
  emit frontLayerChanged(slot, id);
}

void ConstraintModel::putLayerToFront(const SlotId& slot, ossia::none_t)
{
  auto& vec = slot.full_view ? m_fullView : m_smallView;

  vec.at(slot.index).frontProcess = ossia::none;
  emit frontLayerChanged(slot, ossia::none);
}

void ConstraintModel::addSlot(Slot s, SlotId pos)
{
  ISCORE_ASSERT(m_smallView.size() >= pos.index);
  m_smallView.insert(m_smallView.begin() + pos.index, std::move(s));
  emit slotAdded(pos);
}

void ConstraintModel::addSlot(Slot s)
{
  addSlot(std::move(s), {m_smallView.size(), Slot::SmallView});
}

void ConstraintModel::removeSlot(SlotId pos)
{
  ISCORE_ASSERT(m_smallView.size() >= pos.index);
  m_smallView.erase(m_smallView.begin() + pos.index);
  emit slotRemoved(pos);
}

const Slot* ConstraintModel::findSlot(const SlotId& slot) const
{
  if(slot.full_view && slot.index < m_fullView.size())
    return &m_fullView[slot.index];
  else if(slot.index < m_smallView.size())
    return &m_smallView[slot.index];

  return nullptr;
}

const Slot& ConstraintModel::getSlot(const SlotId& slot) const
{
  if(slot.full_view)
    return m_fullView.at(slot.index);
  else
    return m_smallView.at(slot.index);
}

void ConstraintModel::setSlotHeight(const SlotId& slot, double height)
{
  getSlot(slot).height = height;
  emit slotResized(slot);
}

void swap(Scenario::Slot& lhs, Scenario::Slot& rhs)
{
  Scenario::Slot tmp = std::move(lhs);
  lhs = std::move(rhs);
  rhs = std::move(tmp);
}

void ConstraintModel::swapSlots(int pos1, int pos2, Slot::RackView fullview)
{
  auto& vec = fullview ? m_fullView : m_smallView;

  ISCORE_ASSERT(vec.size() > pos1);
  ISCORE_ASSERT(vec.size() > pos2);
  std::iter_swap(vec.begin() + pos1, vec.begin() + pos2);

  emit slotsSwapped(pos1, pos2, fullview);
}

Slot& ConstraintModel::getSlot(const SlotId& slot)
{
  if(slot.full_view)
    return m_fullView.at(slot.index);
  else
    return m_smallView.at(slot.index);
}

void ConstraintModel::on_addProcess(const Process::ProcessModel& p)
{
  // TODO use  m_cmd.context.settings<Scenario::Settings::Model>().getSlotHeight();
  // TODO do it in AddProcess instead.
  m_fullView.push_back(Slot{{p.id()}, {p.id()}, 100});
  emit slotAdded({m_fullView.size() - 1, Slot::FullView});
}

void ConstraintModel::on_removeProcess(const Process::ProcessModel& p)
{
  for(int i = 0; i < m_smallView.size(); i++)
  {
    removeLayer(SlotId{i, Slot::SmallView}, p.id());
  }

  for(int i = 0; i < m_smallView.size(); i++)
  {
    removeLayer(SlotId{i, Slot::FullView}, p.id());
  }

  auto it = ossia::find_if(m_fullView, [] (const auto& slot) {
    return slot.processes.empty();
  });
  if(it != m_fullView.end())
  {
    int N = std::distance(m_fullView.begin(), it);
    m_fullView.erase(it);
    emit slotRemoved(SlotId{N, Slot::FullView});
  }
}

bool isInFullView(const ConstraintModel& cstr)
{
  // TODO just check if parent() == basescenario
  auto& doc = iscore::IDocument::documentContext(cstr);
  auto& sub = safe_cast<Scenario::ScenarioDocumentPresenter&>(
                doc.document.presenter().presenterDelegate());
  return &sub.displayedElements.constraint() == &cstr;
}

bool isInFullView(const Process::ProcessModel& cstr)
{
  return isInFullView(*static_cast<ConstraintModel*>(cstr.parent()));
}

const Scenario::Slot& SlotPath::find() const
{
  return constraint.find().getSlot(*this);
}
const Scenario::Slot* SlotPath::try_find() const
{
  if(auto cst = constraint.try_find())
    return cst->findSlot(*this);
  else
    return nullptr;
}

}
