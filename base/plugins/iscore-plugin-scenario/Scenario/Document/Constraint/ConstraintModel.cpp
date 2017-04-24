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

namespace Scenario
{
class StateModel;
class TimeNodeModel;

ConstraintModel::ConstraintModel(
    const Id<ConstraintModel>& id,
    double yPos,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, ConstraintModel>::get(), parent}
    , m_smallViewRack{new RackModel{smallViewRackId(), this}}
    , m_fullViewRack{new RackModel{fullViewRackId(), this}}
{
  initConnections();
  metadata().setInstanceName(*this);
  metadata().setColor(ScenarioStyle::instance().ConstraintDefaultBackground);
  setHeightPercentage(yPos);

  on_rackAdded(*m_smallViewRack);
  on_rackAdded(*m_fullViewRack);
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

  m_startState = source.startState();
  m_endState = source.endState();
  duration = source.duration;

  m_startDate = source.m_startDate;
  m_heightPercentage = source.heightPercentage();

  // For an explanation of this, see ReplaceConstraintContent command
  std::map<const Process::ProcessModel*, Process::ProcessModel*> processPairs;

  // Clone the processes
  for (const auto& process : source.processes)
  {
    auto newproc = process.clone(process.id(), this);

    processPairs.insert(std::make_pair(&process, newproc));
    processes.add(newproc);

    // We don't need to resize them since the new constraint will have the same
    // duration.
  }

  m_smallViewRack = new RackModel{*source.m_smallViewRack, source.m_smallViewRack->id(), this};
  m_fullViewRack = new RackModel{*source.m_fullViewRack, source.m_fullViewRack->id(), this};
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


void ConstraintModel::on_rackAdded(const RackModel& rack)
{
  con(duration, &ConstraintDurations::defaultDurationChanged, &rack,
      &RackModel::on_durationChanged);
}

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

Id<RackModel> ConstraintModel::smallViewRackId()
{
  return Id<RackModel>{0};
}

Id<RackModel> ConstraintModel::fullViewRackId()
{
  return Id<RackModel>{1};
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

void ConstraintModel::on_addProcess(const Process::ProcessModel& p)
{
  // TODO use  m_cmd.context.settings<Scenario::Settings::Model>().getSlotHeight();
  auto new_fv_slot = new SlotModel{getStrongId(m_fullViewRack->slotmodels), 100, m_fullViewRack};
  m_fullViewRack->addSlot(new_fv_slot);

  new_fv_slot->addLayer(p.id());
}

void ConstraintModel::on_removeProcess(const Process::ProcessModel& p)
{
  for(SlotModel& slot : m_smallViewRack->slotmodels)
  {
    slot.removeLayer(p.id());
  }
  for(SlotModel& slot : m_fullViewRack->slotmodels)
  {
    slot.removeLayer(p.id());
  }
  auto it = ossia::find_if(m_fullViewRack->slotmodels,
                           [] (const auto& slot) {
    return slot.layers().empty();
  });
  if(it != m_fullViewRack->slotmodels.end())
    m_fullViewRack->slotmodels.erase(*it);
}

bool isInFullView(const ConstraintModel& cstr)
{
  auto& doc = iscore::IDocument::documentContext(cstr);
  auto& sub = safe_cast<Scenario::ScenarioDocumentPresenter&>(
                doc.document.presenter().presenterDelegate());
  return &sub.displayedElements.constraint() == &cstr;
}

bool isInFullView(const Process::ProcessModel& cstr)
{
  return isInFullView(*static_cast<ConstraintModel*>(cstr.parent()));
}

}
