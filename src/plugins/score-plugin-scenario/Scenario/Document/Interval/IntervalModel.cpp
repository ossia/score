// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "IntervalModel.hpp"

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Style/ScenarioStyle.hpp>
#include <Process/TimeValue.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/Tempo/TempoProcess.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/ModelMetadata.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>

#include <ossia/detail/ssize.hpp>

#include <wobjectimpl.h>

#include <utility>
W_OBJECT_IMPL(Scenario::IntervalModel)

namespace Scenario
{
class StateModel;
class TimeSyncModel;
IntervalModel::IntervalModel(
    const Id<IntervalModel>& id, double yPos, const score::DocumentContext& ctx,
    QObject* parent)
    : Entity{id, Metadata<ObjectKey_k, IntervalModel>::get(), parent}
    , inlet{std::make_unique<Process::AudioInlet>(
          "Audio In", Id<Process::Port>(0), this)}
    , outlet{std::make_unique<Process::AudioOutlet>(
          "Audio Out", Id<Process::Port>(0), this)}
    , m_context{ctx}
    , m_executionState{}
    , m_viewMode{ViewMode::Temporal}
    , m_smallViewShown{}
    , m_muted{}
    , m_executing{}
    , m_hasSignature{}
    , m_graphal{}
{
  initConnections();
  metadata().setInstanceName(*this);
  const score::Brush& defaultBrush
      = Process::Style::instance().IntervalDefaultBackground();
  metadata().setColor(&defaultBrush);
  setHeightPercentage(yPos);

  outlet->setPropagate(true);
}

IntervalModel::~IntervalModel()
{
  static_assert(
      identified_entity<IntervalModel> && !abstract_base<IntervalModel>
          && !is_custom_serialized<IntervalModel>::value,
      "");
  processes.clear();
  identified_object_destroying(this);
}
void IntervalModel::initConnections()
{
  con(this->duration, &IntervalDurations::speedChanged, this,
      &IntervalModel::ancestorTempoChanged);
  processes.mutable_added.connect<&IntervalModel::on_addProcess>(this);
  processes.removing.connect<&IntervalModel::on_removingProcess>(this);
}

IntervalModel::IntervalModel(
    DataStream::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent)
    : Entity{vis, parent}
    , m_context{ctx}
    , m_executionState{}
    , m_viewMode{ViewMode::Temporal}
    , m_smallViewShown{}
    , m_muted{}
    , m_executing{}
    , m_hasSignature{}
    , m_graphal{}
{
  initConnections();
  vis.writeTo(*this);
}

IntervalModel::IntervalModel(
    JSONObject::Deserializer& vis, const score::DocumentContext& ctx, QObject* parent)
    : Entity{vis, parent}
    , m_context{ctx}
    , m_executionState{}
    , m_viewMode{ViewMode::Temporal}
    , m_smallViewShown{}
    , m_muted{}
    , m_executing{}
    , m_hasSignature{}
    , m_graphal{}
{
  initConnections();
  vis.writeTo(*this);
}

IntervalModel::IntervalModel(
    DataStream::Deserializer&& vis, const score::DocumentContext& ctx, QObject* parent)
    : Entity{vis, parent}
    , m_context{ctx}
    , m_executionState{}
    , m_viewMode{ViewMode::Temporal}
    , m_smallViewShown{}
    , m_muted{}
    , m_executing{}
    , m_hasSignature{}
    , m_graphal{}
{
  initConnections();
  vis.writeTo(*this);
}

IntervalModel::IntervalModel(
    JSONObject::Deserializer&& vis, const score::DocumentContext& ctx, QObject* parent)
    : Entity{vis, parent}
    , m_context{ctx}
    , m_executionState{}
    , m_viewMode{ViewMode::Temporal}
    , m_smallViewShown{}
    , m_muted{}
    , m_executing{}
    , m_hasSignature{}
    , m_graphal{}
{
  initConnections();
  vis.writeTo(*this);
}

void IntervalModel::setHasTimeSignature(bool b)
{
  if(b != m_hasSignature)
  {
    m_hasSignature = b;
    if(b && m_signatures.empty())
    {
      m_signatures[TimeVal::zero()] = {4, 4};
    }
    hasTimeSignatureChanged(b);
  }
}

TimeVal IntervalModel::contentDuration() const noexcept
{
  TimeVal min_time
      = (duration.isMaxInfinite() ? duration.defaultDuration() : duration.maxDuration());

  for(Process::ProcessModel& proc : processes)
  {
    if(!(proc.flags() & Process::TimeIndependent))
    {
      if(auto d = proc.contentDuration(); d > min_time)
        min_time = d;
    }
  }

  return min_time * 1.1;
}

TempoProcess* IntervalModel::tempoCurve() const noexcept
{
  for(auto& proc : processes)
  {
    if(auto tempo = qobject_cast<TempoProcess*>(&proc))
    {
      return tempo;
    }
  }
  return nullptr;
}

void IntervalModel::ancestorStartDateChanged()
{
  for(auto& proc : processes)
    proc.ancestorStartDateChanged();
}

void IntervalModel::ancestorTempoChanged()
{
  if(!tempoCurve())
  {
    for(auto& proc : processes)
      proc.ancestorTempoChanged();
  }
}

void IntervalModel::addSignature(TimeVal t, ossia::time_signature sig)
{
  m_signatures[t] = sig;
  timeSignaturesChanged(m_signatures);
}

void IntervalModel::removeSignature(TimeVal t)
{
  m_signatures.erase(t);
  timeSignaturesChanged(m_signatures);
}
const TimeSignatureMap& IntervalModel::timeSignatureMap() const noexcept
{
  return m_signatures;
}

void IntervalModel::setTimeSignatureMap(const TimeSignatureMap& map)
{
  // qDebug() << "New map";
  // for(auto& [a,b]: map)
  //   qDebug() << a.msec() << b.upper << "/" << b.lower;
  if(map != m_signatures)
  {
    m_signatures = map;
    timeSignaturesChanged(map);
  }
}

ossia::musical_sync IntervalModel::quantizationRate() const noexcept
{
  return m_quantRate;
}

void IntervalModel::setQuantizationRate(ossia::musical_sync b)
{
  if(b != m_quantRate)
  {
    m_quantRate = b;
    quantizationRateChanged(b);
  }
}

const Id<StateModel>& IntervalModel::startState() const noexcept
{
  return m_startState;
}

void IntervalModel::setStartState(const Id<StateModel>& e)
{
  m_startState = e;
}

const Id<StateModel>& IntervalModel::endState() const noexcept
{
  return m_endState;
}

void IntervalModel::setEndState(const Id<StateModel>& endState)
{
  m_endState = endState;
}

const TimeVal& IntervalModel::date() const noexcept
{
  return m_date;
}

void IntervalModel::setStartDate(const TimeVal& start)
{
  m_date = start;

  for(auto& proc : processes)
    proc.ancestorStartDateChanged();

  dateChanged(start);
}

void IntervalModel::translate(const TimeVal& deltaTime)
{
  setStartDate(m_date + deltaTime);
}

// Simple getters and setters

double IntervalModel::heightPercentage() const noexcept
{
  return m_heightPercentage;
}

// Should go in an "execution" object.
void IntervalModel::startExecution()
{
  for(Process::ProcessModel& proc : processes)
  {
    proc.setExecuting(true);
    proc.startExecution(); // prevents editing
  }
}
void IntervalModel::stopExecution()
{
  duration.setPlayPercentage(0);
  if(!m_hasSignature)
    duration.setSpeed(1.0);

  for(Process::ProcessModel& proc : processes)
  {
    proc.setExecuting(false);
    proc.stopExecution();
  }
  setExecuting(false);
}

void IntervalModel::reset()
{
  duration.setPlayPercentage(0);

  if(!m_hasSignature)
    duration.setSpeed(1.0);

  for(Process::ProcessModel& proc : processes)
  {
    proc.setExecuting(false);
    proc.resetExecution();
    proc.stopExecution();
  }

  setExecutionState(IntervalExecutionState::Enabled);
  setExecuting(false);
}

void IntervalModel::setHeightPercentage(double arg)
{
  if(m_heightPercentage != arg)
  {
    m_heightPercentage = arg;
    heightPercentageChanged(arg);
  }
}

void IntervalModel::setExecutionState(IntervalExecutionState s)
{
  if(s != m_executionState)
  {
    m_executionState = s;
    executionStateChanged(executionState());
  }
}

IntervalExecutionState IntervalModel::executionState() const
{
  switch(m_executionState)
  {
    case IntervalExecutionState::Enabled:
      return m_muted ? IntervalExecutionState::Muted : IntervalExecutionState::Enabled;
    default:
      return m_executionState;
  }
}

IntervalModel::ViewMode IntervalModel::viewMode() const noexcept
{
  return m_viewMode;
}

void IntervalModel::setViewMode(IntervalModel::ViewMode v)
{
  if(v != m_viewMode)
  {
    m_viewMode = v;
    viewModeChanged(v);
  }
}

QPointF IntervalModel::nodalOffset() const noexcept
{
  return m_nodalOffset;
}

void IntervalModel::setNodalOffset(QPointF offs)
{
  if(m_nodalOffset != offs)
  {
    m_nodalOffset = offs;
    nodalOffsetChanged(m_nodalOffset);
  }
}

double IntervalModel::nodalScale() const noexcept
{
  return m_nodalScale;
}

void IntervalModel::setNodalScale(double zoom)
{
  if(m_nodalScale != zoom)
  {
    m_nodalScale = zoom;
    nodalScaleChanged(m_nodalScale);
  }
}

ZoomRatio IntervalModel::zoom() const noexcept
{
  return m_zoom;
}

void IntervalModel::setZoom(const ZoomRatio& zoom)
{
  m_zoom = zoom;
}

TimeVal IntervalModel::midTime() const noexcept
{
  return m_center;
}

void IntervalModel::setMidTime(const TimeVal& value)
{
  m_center = value;
}

void IntervalModel::setSmallViewVisible(bool v)
{
  if(v != m_smallViewShown)
  {
    m_smallViewShown = v;
    smallViewVisibleChanged(v);
  }
}

bool IntervalModel::smallViewVisible() const noexcept
{
  return m_smallViewShown;
}

void IntervalModel::clearSmallView()
{
  m_smallView.clear();
  rackChanged(Slot::SmallView);
}

void IntervalModel::clearFullView()
{
  m_fullView.clear();
  rackChanged(Slot::FullView);
}

void IntervalModel::replaceSmallView(const Rack& other)
{
  m_smallView = other;
  rackChanged(Slot::SmallView);
}

void IntervalModel::replaceFullView(const FullRack& other)
{
  m_fullView = other;
  rackChanged(Slot::FullView);
}

void IntervalModel::addLayer(int slot, Id<Process::ProcessModel> id)
{
  auto& procs = m_smallView.at(slot).processes;
  SCORE_ASSERT(ossia::find(procs, id) == procs.end());
  procs.push_back(id);

  layerAdded({slot, Slot::SmallView}, id);

  putLayerToFront(slot, id);
}

void IntervalModel::removeLayer(int slot, Id<Process::ProcessModel> id)
{
  auto& procs = m_smallView.at(slot).processes;
  const auto N = procs.size();
  ossia::remove_erase(procs, id);

  if(procs.size() < N)
  {
    layerRemoved({slot, Slot::SmallView}, id);

    if(!procs.empty())
      putLayerToFront(slot, procs.front());
    else
      putLayerToFront(slot, std::nullopt);
  }
}

void IntervalModel::putLayerToFront(int slot, Id<Process::ProcessModel> id)
{
  m_smallView.at(slot).frontProcess = id;
  frontLayerChanged(slot, id);
}

void IntervalModel::putLayerToFront(int slot, std::nullopt_t)
{
  m_smallView.at(slot).frontProcess = std::nullopt;
  frontLayerChanged(slot, std::nullopt);
}

void IntervalModel::addSlot(Slot s, int pos)
{
  SCORE_ASSERT(std::ssize(m_smallView) >= pos);
  m_smallView.insert(m_smallView.begin() + pos, std::move(s));
  slotAdded({pos, Slot::SmallView});

  if(m_smallView.size() == 1)
    setSmallViewVisible(true);
}

void IntervalModel::addSlot(Slot s)
{
  addSlot(std::move(s), m_smallView.size());
}

void IntervalModel::removeSlot(int pos)
{
  if(std::ssize(m_smallView) > pos)
  {
    m_smallView.erase(m_smallView.begin() + pos);
    slotRemoved({pos, Slot::SmallView});

    if(m_smallView.empty())
      setSmallViewVisible(false);
  }
}

const Slot* IntervalModel::findSmallViewSlot(int slot) const
{
  if(slot < std::ssize(m_smallView))
    return &m_smallView[slot];

  return nullptr;
}

const Slot& IntervalModel::getSmallViewSlot(int slot) const
{
  return m_smallView.at(slot);
}

Slot& IntervalModel::getSmallViewSlot(int slot)
{
  return m_smallView.at(slot);
}

const FullSlot* IntervalModel::findFullViewSlot(int slot) const
{
  if(slot < std::ssize(m_fullView))
    return &m_fullView[slot];

  return nullptr;
}

const FullSlot& IntervalModel::getFullViewSlot(int slot) const
{
  return m_fullView.at(slot);
}

FullSlot& IntervalModel::getFullViewSlot(int slot)
{
  return m_fullView.at(slot);
}

void IntervalModel::setMuted(bool m)
{
  if(m != m_muted)
  {
    m_muted = m;
    mutedChanged(m);
    executionStateChanged(executionState());
  }
}

void IntervalModel::setGraphal(bool m)
{
  if(m != m_graphal)
  {
    SCORE_ASSERT(!m_graphal); // once an interval is set graphal it cannot go back
    // TODO this should be a ctor thing instead...
    m_graphal = m;
    inlet.reset();
    outlet.reset();
    graphalChanged(m);
  }
}

void IntervalModel::setExecuting(bool m)
{
  if(m != m_executing)
  {
    m_executing = m;
    executingChanged(m);
    if(m_executing)
      startExecution();
    else
      stopExecution();
  }
}

void IntervalModel::setStartMarker(TimeVal t)
{
  if(t != m_startMarker)
  {
    m_startMarker = t;
    startMarkerChanged(t);
  }
}

TimeVal IntervalModel::startMarker() const noexcept
{
  return m_startMarker;
}

double IntervalModel::getSlotHeight(const SlotId& slot) const
{
  if(slot.fullView())
  {
    auto& slt = m_fullView.at(slot.index);
    if(slt.nodal)
    {
      return m_nodalFullViewSlotHeight;
    }
    else
    {
      auto it = processes.find(slt.process);
      if(it != processes.end())
        return it->getSlotHeight();
      else
        return 0.;
    }
  }
  else
  {
    return m_smallView.at(slot.index).height;
  }
}

double IntervalModel::getSlotHeightForProcess(const Id<Process::ProcessModel>& p) const
{
  for(auto& slt : m_smallView)
  {
    for(auto& proc : slt.processes)
    {
      if(proc == p)
        return slt.height;
    }
  }

  return 0.;
}

void IntervalModel::setSlotHeight(const SlotId& slot, double height)
{
  height = std::max(height, 20.);
  if(slot.fullView())
  {
    auto& slt = m_fullView.at(slot.index);
    if(slt.nodal)
    {
      m_nodalFullViewSlotHeight = height;
    }
    else
    {
      auto it = processes.find(slt.process);
      if(it != processes.end())
        it->setSlotHeight(height);
    }
  }
  else
  {
    getSmallViewSlot(slot.index).height = height;
  }
  slotResized(slot);
}

double IntervalModel::getHeight() const noexcept
{
  double h = 0.;
  const double slotSize = (this->smallViewVisible() ? 1. : 0.);
  for(const auto& slot : m_smallView)
  {
    h += slot.height * slotSize + SlotHeader::headerHeight()
         + SlotFooter::footerHeight();
  }
  return h;
}

void swap(Scenario::Slot& lhs, Scenario::Slot& rhs)
{
  Scenario::Slot tmp = std::move(lhs);
  lhs = std::move(rhs);
  rhs = std::move(tmp);
}

void IntervalModel::swapSlots(int pos1, int pos2, Slot::RackView v)
{
  SCORE_ASSERT(pos1 >= 0);
  SCORE_ASSERT(pos2 >= 0);
  if(v == Slot::FullView)
  {
    auto& v = m_fullView;
    int N = std::ssize(v);
    if(pos1 < N && pos2 < N)
    {
      if(pos1 < pos2)
      {
        auto val = *(v.begin() + pos1);

        v.insert(v.begin() + pos2, val);
        v.erase(v.begin() + pos1);
      }
      else if(pos1 > pos2)
      {
        auto val = *(v.begin() + pos1);

        v.insert(v.begin() + pos2, val);
        v.erase(v.begin() + pos1 + 1);
      }
      // std::swap(*(v.begin() + pos1), *(v.begin() + pos2));
    }
    else if(pos1 < N && pos2 >= N)
    {
      auto it = v.begin() + pos1;
      std::rotate(it, it + 1, v.end());
    }
    else if(pos2 < N && pos1 >= N)
    {
      auto it = v.begin() + pos2;
      std::rotate(it, it + 1, v.end());
    }
  }
  else
  {
    auto& v = m_smallView;
    int N = std::ssize(v);
    if(pos1 < N && pos2 < N)
    {
      if(pos1 < pos2)
      {
        auto val = *(v.begin() + pos1);

        v.insert(v.begin() + pos2, val);
        v.erase(v.begin() + pos1);
      }
      else if(pos1 > pos2)
      {
        auto val = *(v.begin() + pos1);

        v.insert(v.begin() + pos2, val);
        v.erase(v.begin() + pos1 + 1);
      }
      // std::swap(*(v.begin() + pos1), *(v.begin() + pos2));
    }
    else if(pos1 < N && pos2 >= N)
    {
      auto it = v.begin() + pos1;
      std::rotate(it, it + 1, v.end());
    }
    else if(pos2 < N && pos1 >= N)
    {
      auto it = v.begin() + pos2;
      std::rotate(it, it + 1, v.end());
    }
  }
  slotsSwapped(pos1, pos2, v);
}

void IntervalModel::on_addProcess(Process::ProcessModel& p)
{
  if(!(p.flags() & Process::ProcessFlags::TimeIndependent))
  {
    m_fullView.push_back(FullSlot{p.id()});
    slotAdded({m_fullView.size() - 1, Slot::FullView});
  }
  else
  {
    const auto smallNodalSlot
        = ossia::find_if(m_smallView, [](const auto& slt) { return slt.nodal; });
    if(smallNodalSlot == m_smallView.end())
    {
      Slot slt;
      slt.nodal = true;
      slt.height = context().app.settings<Scenario::Settings::Model>().getSlotHeight();
      slt.processes.push_back(p.id());
      m_smallView.push_back(std::move(slt));
      slotAdded({m_smallView.size() - 1, Slot::SmallView});
    }
    else
    {
      smallNodalSlot->processes.push_back(p.id());
    }

    const bool fullNodalSlot
        = ossia::any_of(m_fullView, [](const auto& slt) { return slt.nodal; });
    if(!fullNodalSlot)
    {
      FullSlot slt;
      slt.nodal = true;
      m_fullView.push_back(std::move(slt));
      slotAdded({m_fullView.size() - 1, Slot::FullView});
    }
    else
    {
      // todo ! ?
    }
  }

  con(metadata(), &score::ModelMetadata::ColorChanged, &p.metadata(),
      &score::ModelMetadata::setColor);
  p.metadata().setColor(metadata().getColor());
}

void IntervalModel::on_removingProcess(const Process::ProcessModel& p)
{
  if(!(p.flags() & Process::ProcessFlags::TimeIndependent))
  {
    const auto& pid = p.id();
    for(int i = 0; i < std::ssize(m_smallView); i++)
    {
      removeLayer(i, pid);
    }

    auto it = ossia::find_if(
        m_fullView, [&](const FullSlot& slot) { return slot.process == pid; });
    if(it != m_fullView.end())
    {
      int N = std::distance(m_fullView.begin(), it);
      m_fullView.erase(it);
      slotRemoved(SlotId{N, Slot::FullView});
    }
  }
  else
  {
    {
      const auto smallNodalSlot
          = ossia::find_if(m_smallView, [](const auto& slt) { return slt.nodal; });

      if(smallNodalSlot != m_smallView.end())
      {
        SCORE_ASSERT(m_smallView.size() > 0);
        SCORE_ASSERT(!m_smallView.empty());
        if(smallNodalSlot->processes.size() > 1)
        {
          ossia::remove_erase(smallNodalSlot->processes, p.id());
        }
        else
        {
          int N = std::distance(m_smallView.begin(), smallNodalSlot);
          removeSlot(N);
        }
      }
    }
    {
      const auto fullNodalSlot
          = ossia::find_if(m_fullView, [](const auto& slt) { return slt.nodal; });

      if(fullNodalSlot != m_fullView.end())
      {
        SCORE_ASSERT(m_fullView.size() > 0);
        int numTimeIndependent = ossia::count_if(processes, [](const auto& proc) {
          return proc.flags() & Process::ProcessFlags::TimeIndependent;
        });
        if(numTimeIndependent <= 1)
        {
          int N = std::distance(m_fullView.begin(), fullNodalSlot);
          m_fullView.erase(fullNodalSlot);
          slotRemoved(SlotId{N, Slot::FullView});
        }
        /* TODO
        if(fullNodalSlot->processes.size() > 1)
        {
          ossia::remove_erase(fullNodalSlot->processes, p.id());
        }
        else
        {
          int N = std::distance(m_fullView.begin(), fullNodalSlot);
          m_fullView.erase(fullNodalSlot);
          slotRemoved(SlotId{N, Slot::fullView});
        }
        */
      }
    }
  }
}

bool isInFullView(const IntervalModel& cstr) noexcept
{
  if(qobject_cast<BaseScenario*>(cstr.parent()))
    return true;

  auto& doc = score::IDocument::documentContext(cstr);
  if(auto pres = doc.document.presenter())
  {
    auto sub
        = qobject_cast<Scenario::ScenarioDocumentPresenter*>(pres->presenterDelegate());
    if(sub)
      return &sub->displayedElements.interval() == &cstr;
    return false;
  }
  return false;
}

bool isInFullView(const Process::ProcessModel& cstr) noexcept
{
  return isInFullView(*static_cast<IntervalModel*>(cstr.parent()));
}

const Scenario::Slot& SlotPath::find(const score::DocumentContext& ctx) const
{
  return interval.find(ctx).getSmallViewSlot(index);
}

const Scenario::Slot* SlotPath::try_find(const score::DocumentContext& ctx) const
{
  if(auto cst = interval.try_find(ctx))
    return cst->findSmallViewSlot(index);
  else
    return nullptr;
}

bool isBus(const IntervalModel& model, const score::DocumentContext& ctx) noexcept
{
  auto& buses = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document)
                    .busIntervals;
  return ossia::contains(buses, &model);
}

ParentTimeInfo closestParentWithMusicalMetrics(const IntervalModel* self)
{
  TimeVal delta = TimeVal::zero();
  if(self->hasTimeSignature())
    return {self, self, delta};

  delta += self->date();
  auto p = self->parent();
  if(p)
  {
    p = p->parent();
  }

  const IntervalModel* lastFound = self;
  while(p)
  {
    if(auto pi = qobject_cast<const IntervalModel*>(p))
    {
      lastFound = pi;
      if(pi->hasTimeSignature())
      {
        return {pi, pi, delta};
      }
      else
      {
        delta += pi->date();
        if((p = p->parent()))
          p = p->parent();
      }
    }
    else
    {
      if((p = p->parent()))
        p = p->parent();
    }
  }
  return {nullptr, lastFound, {}};
}

ParentTimeInfo closestParentWithQuantification(const IntervalModel* self)
{
  TimeVal delta = TimeVal::zero();
  if(self->quantizationRate() >= 0.)
    return {self, self, delta};

  delta += self->date();
  auto p = self->parent();
  if(p)
  {
    p = p->parent();
  }

  const IntervalModel* lastFound = self;
  while(p)
  {
    if(auto pi = qobject_cast<const IntervalModel*>(p))
    {
      lastFound = pi;
      if(pi->quantizationRate() >= 0.)
      {
        return {pi, pi, delta};
      }
      else
      {
        delta += pi->date();
        if((p = p->parent()))
          p = p->parent();
      }
    }
    else
    {
      if((p = p->parent()))
        p = p->parent();
    }
  }
  return {nullptr, lastFound, {}};
}

ParentTimeInfo closestParentWithTempo(const IntervalModel* self)
{
  TimeVal delta = TimeVal::zero();
  if(self->tempoCurve())
    return {self, self, delta};

  delta += self->date();
  auto p = self->parent();
  if(p)
  {
    p = p->parent();
  }

  const IntervalModel* lastFound = self;
  ;
  while(p)
  {
    if(auto pi = qobject_cast<const IntervalModel*>(p))
    {
      lastFound = pi;
      if(pi->tempoCurve())
      {
        return {pi, pi, delta};
      }
      else
      {
        delta += pi->date();
        if((p = p->parent()))
          p = p->parent();
      }
    }
    else
    {
      if((p = p->parent()))
        p = p->parent();
    }
  }

  return {nullptr, lastFound, {}};
}

QPointF newProcessPosition(const IntervalModel& cst) noexcept
{
  static ossia::flat_set<double> autoPos;
  autoPos.clear();
  autoPos.reserve(100);
  for(const Process::ProcessModel& proc : cst.processes)
  {
    const auto p = proc.position();
    if(p.x() - p.y() < 5)
    {
      autoPos.insert((p.x() + p.y()) / 2);
    }
  }

  double start = 40.;
  auto it = autoPos.lower_bound(start);
  double distance = 0.;
  if(it != autoPos.end())
  {
    distance = std::abs(*it - start);
    if(distance < 10)
    {
      do
      {
        start += 10.;
        it = autoPos.lower_bound(start);
        if(it != autoPos.end())
          distance = std::abs(*it - start);
      } while(it != autoPos.end() && distance < 10);

      if(distance < 10)
        start += 10;
    }
  }
  return {start, start};
}

// TODO refactor by grepping for _cast.*IntervalModel
IntervalModel* closestParentInterval(const QObject* parentObj) noexcept
{
  while(parentObj && !qobject_cast<const Scenario::IntervalModel*>(parentObj))
  {
    parentObj = parentObj->parent();
  }
  return static_cast<IntervalModel*>(const_cast<QObject*>(parentObj));
}

TimeVal timeDelta(const IntervalModel* self, const IntervalModel* parent)
{
  TimeVal delta = TimeVal::zero();
  if(self == parent)
    return delta;

  delta += self->date();
  auto p = self->parent();
  if(p)
  {
    p = p->parent();
  }

  while(p)
  {
    if(auto pi = qobject_cast<const IntervalModel*>(p))
    {
      if(pi == parent)
      {
        return delta;
      }
      else
      {
        delta += pi->date();
        if((p = p->parent()))
          p = p->parent();
      }
    }
    else
    {
      if((p = p->parent()))
        p = p->parent();
    }
  }

  return TimeVal{};
}

}
