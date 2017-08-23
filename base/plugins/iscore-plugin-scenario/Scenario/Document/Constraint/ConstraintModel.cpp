// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Scenario/Document/Constraint/Slot.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
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
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/tools/Todo.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <Dataflow/UI/NodeItem.hpp>
#include <Dataflow/UI/Slider.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <Dataflow/DataflowWindow.hpp>
namespace Scenario
{

ConstraintNode::ConstraintNode(QObject *parent)
  : Process::Node{Id<Node>{}, parent}
{
}

QString ConstraintNode::getText() const
{
    return tr("Constraint");
}

std::size_t ConstraintNode::audioInlets() const
{
    return 1;
}

std::size_t ConstraintNode::messageInlets() const
{
    return 1;
}

std::size_t ConstraintNode::midiInlets() const
{
    return 1;
}

std::size_t ConstraintNode::audioOutlets() const
{
    return 1;
}

std::size_t ConstraintNode::messageOutlets() const
{
    return 1;
}

std::size_t ConstraintNode::midiOutlets() const
{
    return 1;
}

std::vector<Process::Port> ConstraintNode::inlets() const
{
    std::vector<Process::Port> p(3);
    p[0].type = Process::PortType::Audio;
    p[1].type = Process::PortType::Message;
    p[2].type = Process::PortType::Midi;
    return p;
}

std::vector<Process::Port> ConstraintNode::outlets() const
{
  std::vector<Process::Port> p(3);
  p[0].type = Process::PortType::Audio;
  p[1].type = Process::PortType::Message;
  p[2].type = Process::PortType::Midi;
  for(auto& port : p)
    port.propagate = true;
  return p;
}

std::vector<Id<Process::Cable> > ConstraintNode::cables() const
{ return m_cables; }

void ConstraintNode::addCable(Id<Process::Cable> c)
{ m_cables.push_back(c); }

void ConstraintNode::removeCable(Id<Process::Cable> c)
{ m_cables.erase(ossia::find(m_cables, c)); }



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
  processes.removing
      .connect<ConstraintModel, &ConstraintModel::on_removingProcess>(this);
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

TimeVal ConstraintModel::midTime() const
{
  return m_center;
}

void ConstraintModel::setMidTime(const TimeVal& value)
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

void ConstraintModel::replaceFullView(const FullRack& other)
{
  m_fullView = other;
  emit rackChanged(Slot::FullView);
}

void ConstraintModel::addLayer(int slot, Id<Process::ProcessModel> id)
{
  auto& procs = m_smallView.at(slot).processes;
  ISCORE_ASSERT(ossia::find(procs, id) == procs.end());
  procs.push_back(id);

  emit layerAdded({slot, Slot::SmallView}, id);

  putLayerToFront(slot, id);
}

void ConstraintModel::removeLayer(int slot, Id<Process::ProcessModel> id)
{
  auto& procs = m_smallView.at(slot).processes;
  const auto N = procs.size();
  boost::range::remove_erase(procs, id);

  if(procs.size() < N)
  {
    emit layerRemoved({slot, Slot::SmallView}, id);

    if(!procs.empty())
      putLayerToFront(slot, procs.front());
    else
      putLayerToFront(slot, ossia::none);
  }
}

void ConstraintModel::putLayerToFront(int slot, Id<Process::ProcessModel> id)
{
  m_smallView.at(slot).frontProcess = id;
  emit frontLayerChanged(slot, id);
}

void ConstraintModel::putLayerToFront(int slot, ossia::none_t)
{
  m_smallView.at(slot).frontProcess = ossia::none;
  emit frontLayerChanged(slot, ossia::none);
}

void ConstraintModel::addSlot(Slot s, int pos)
{
  ISCORE_ASSERT((int)m_smallView.size() >= pos);
  m_smallView.insert(m_smallView.begin() + pos, std::move(s));
  emit slotAdded({pos, Slot::SmallView});

  if(m_smallView.size() == 1)
    setSmallViewVisible(true);
}

void ConstraintModel::addSlot(Slot s)
{
  addSlot(std::move(s), m_smallView.size());
}

void ConstraintModel::removeSlot(int pos)
{
  ISCORE_ASSERT((int)m_smallView.size() >= pos);
  m_smallView.erase(m_smallView.begin() + pos);
  emit slotRemoved({pos, Slot::SmallView});

  if(m_smallView.empty())
    setSmallViewVisible(false);
}


const Slot* ConstraintModel::findSmallViewSlot(int slot) const
{
  if(slot < (int)m_smallView.size())
    return &m_smallView[slot];

  return nullptr;
}

const Slot& ConstraintModel::getSmallViewSlot(int slot) const
{
  return m_smallView.at(slot);
}

Slot& ConstraintModel::getSmallViewSlot(int slot)
{
  return m_smallView.at(slot);
}



const FullSlot* ConstraintModel::findFullViewSlot(int slot) const
{
  if(slot < (int)m_fullView.size())
    return &m_fullView[slot];

  return nullptr;
}

const FullSlot& ConstraintModel::getFullViewSlot(int slot) const
{
  return m_fullView.at(slot);
}

FullSlot& ConstraintModel::getFullViewSlot(int slot)
{
  return m_fullView.at(slot);
}

double ConstraintModel::getSlotHeight(const SlotId& slot) const
{
  if(slot.fullView())
    return processes.at(m_fullView.at(slot.index).process).getSlotHeight();
  else
    return m_smallView.at(slot.index).height;
}


void ConstraintModel::setSlotHeight(const SlotId& slot, double height)
{
  height = std::max(height, 20.);
  if(slot.fullView())
    processes.at(m_fullView.at(slot.index).process).setSlotHeight(height);
  else
    getSmallViewSlot(slot.index).height = height;
  emit slotResized(slot);
}

void swap(Scenario::Slot& lhs, Scenario::Slot& rhs)
{
  Scenario::Slot tmp = std::move(lhs);
  lhs = std::move(rhs);
  rhs = std::move(tmp);
}

void ConstraintModel::swapSlots(int pos1, int pos2, Slot::RackView v)
{
  if(v == Slot::FullView)
  {
    auto& vec = m_fullView;
    ISCORE_ASSERT((int)vec.size() > pos1);
    ISCORE_ASSERT((int)vec.size() > pos2);
    std::iter_swap(vec.begin() + pos1, vec.begin() + pos2);
  }
  else
  {
    auto& vec = m_smallView;
    ISCORE_ASSERT((int)vec.size() > pos1);
    ISCORE_ASSERT((int)vec.size() > pos2);
    std::iter_swap(vec.begin() + pos1, vec.begin() + pos2);
  }
  emit slotsSwapped(pos1, pos2, v);
}

void ConstraintModel::on_addProcess(const Process::ProcessModel& p)
{
  m_fullView.push_back(FullSlot{p.id()});
  emit slotAdded({m_fullView.size() - 1, Slot::FullView});
}

void ConstraintModel::on_removingProcess(const Process::ProcessModel& p)
{
  const auto& pid = p.id();
  for(int i = 0; i < (int)m_smallView.size(); i++)
  {
    removeLayer(i, pid);
  }

  auto it = ossia::find_if(m_fullView, [&] (const FullSlot& slot) {
    return slot.process == pid;
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

const Scenario::Slot& SlotPath::find(const iscore::DocumentContext& ctx) const
{
  return constraint.find(ctx).getSmallViewSlot(index);
}

const Scenario::Slot* SlotPath::try_find(const iscore::DocumentContext& ctx) const
{
  if(auto cst = constraint.try_find(ctx))
    return cst->findSmallViewSlot(index);
  else
    return nullptr;
}

}
