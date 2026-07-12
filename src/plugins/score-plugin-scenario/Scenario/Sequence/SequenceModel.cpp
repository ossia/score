// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceModel.hpp"

#include <Automation/AutomationModel.hpp>
#include <Automation/AutomationProcessMetadata.hpp>

#include <Color/GradientModel.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Interval/IntervalDurations.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/Algorithms/ProcessPolicy.hpp>

#include <Process/State/MessageNode.hpp>

#include <State/Message.hpp>
#include <State/ValueConversion.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/common/destination_qualifiers.hpp>
#include <ossia/network/dataspace/dataspace_variant_visitors.hpp>
#include <ossia/network/dataspace/dataspace_visitors.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QColor>

#include <Process/TimeValue.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Sequence::SequenceModel)

namespace Sequence
{

// Default height of the small-view slot holding a section's automations.
static constexpr qreal kSectionSlotHeight = 100.;

// RAII guard for m_rackSync: restores the previous value so nested structural
// operations behave.
struct RackSyncGuard
{
  bool& flag;
  bool prev;
  explicit RackSyncGuard(bool& f)
      : flag{f}
      , prev{f}
  {
    flag = true;
  }
  ~RackSyncGuard() { flag = prev; }
};

// The per-section instances' own ports are not shown: the sequence exposes
// one port per parameter for the whole sequence, drawn per slot row.
static void hideInstancePorts(Process::ProcessModel& proc)
{
  for(auto* in : proc.inlets())
    in->displayHandledExplicitly = true;
  for(auto* out : proc.outlets())
    out->displayHandledExplicitly = true;
}

// All of a section's automations pile up in one slot by default; the user
// reorganizes them (and the layout mirrors across sections) from there.
static void addProcessToSectionRack(
    Scenario::IntervalModel& itv, const Id<Process::ProcessModel>& procId)
{
  if(itv.smallView().empty())
    itv.addSlot(Scenario::Slot{{procId}, procId, kSectionSlotHeight});
  else
    itv.addLayer(0, procId);
  itv.setSmallViewVisible(true);
}

// ---- Color parameter helpers ----
// A parameter whose address carries a color unit is automated with a
// Gradient process instead of an Automation.

static const ossia::color_u* colorUnit(const State::AddressAccessor& addr)
{
  return addr.qualifiers.get().unit.v.target<ossia::color_u>();
}

// Mirrors the converter used by the legacy auto-sequence path.
struct color_to_qcolor
{
  template <typename Color>
  QColor operator()(const typename Color::value_type& value, const Color&)
  {
    auto rgba = ossia::rgba{ossia::strong_value<Color>{value}};
    auto& col = rgba.dataspace_value;
    return QColor::fromRgbF((qreal)col[0], (qreal)col[1], (qreal)col[2], (qreal)col[3]);
  }

  template <typename... Args>
  QColor operator()(Args&&...)
  {
    return QColor{};
  }
};

static QColor valueToColor(const ossia::value& v, const ossia::color_u& u)
{
  QColor c = ossia::apply(color_to_qcolor{}, v.v, u);
  if(!c.isValid())
  {
    // Device values may be stored as generic lists instead of vec4f;
    // coerce and retry, otherwise the converter's fallback yields an
    // invalid color which renders black.
    const ossia::value coerced{ossia::convert<ossia::vec4f>(v)};
    c = ossia::apply(color_to_qcolor{}, coerced.v, u);
  }
  return c.isValid() ? c : QColor::fromRgbF(0., 0., 0., 1.);
}

// QColor → ossia value expressed in the address's own color unit.
static ossia::value colorToValue(const QColor& c, const ossia::color_u& u)
{
  ossia::rgba col{
      (float)c.redF(), (float)c.greenF(), (float)c.blueF(), (float)c.alphaF()};
  return ossia::to_value(ossia::convert(col, ossia::unit_t{u}));
}

static QColor lerpColor(const QColor& a, const QColor& b, double t)
{
  return QColor::fromRgbF(
      a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
      a.blueF() + (b.blueF() - a.blueF()) * t,
      a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

// Interpolated color of a gradient at position t (same linear semantics as the
// gradient rendering: flat before the first and after the last stop).
static QColor
gradientValueAt(const ossia::flat_map<double, QColor>& stops, double t)
{
  if(stops.empty())
    return QColor::fromRgbF(0., 0., 0., 1.);
  auto it = stops.lower_bound(t);
  if(it == stops.begin())
    return it->second;
  if(it == stops.end())
    return std::prev(it)->second;
  if(it->first == t)
    return it->second;
  auto prev = std::prev(it);
  const double span = it->first - prev->first;
  const double u = span < 1e-9 ? 0. : (t - prev->first) / span;
  return lerpColor(prev->second, it->second, u);
}

// Normalize a scalar into [0,1] against a range, clamped.
static double normalizeScalar(double v, double minVal, double maxVal)
{
  const double range = maxVal - minVal;
  if(range < 1e-9)
    return 0.5;
  double norm = (v - minVal) / range;
  if(norm < 0.0)
    norm = 0.0;
  if(norm > 1.0)
    norm = 1.0;
  return norm;
}

// Helper: create a single linear automation segment from y0 to y1 (normalized).
// The Automation constructor installs a default 0→1 ramp segment; clear it first,
// otherwise both segments coexist and the curve renders/executes garbage.
static void initSegmentCurve(Curve::Model& curve, double y0, double y1)
{
  curve.clear();
  auto seg = new Curve::LinearSegment(Id<Curve::SegmentModel>(0), &curve);
  seg->setStart({0.0, y0});
  seg->setEnd({1.0, y1});
  curve.addSegment(seg);
}

static void initFlatCurve(Curve::Model& curve, double normY)
{
  initSegmentCurve(curve, normY, normY);
}

// ---- Polyline helpers for curve splitting / merging ----
// Curves are flattened to their segment endpoints (exact for the linear
// segments the sequence generates; curved segments are linearized).

static QVector<QPointF> curveToPolyline(const Curve::Model& curve)
{
  QVector<QPointF> pts;
  const auto segs = curve.sortedSegments();
  for(const auto* seg : segs)
  {
    if(pts.empty() || pts.back().x() < seg->start().x())
      pts.push_back({seg->start().x(), seg->start().y()});
    pts.push_back({seg->end().x(), seg->end().y()});
  }
  if(pts.empty())
    pts = {{0., 0.}, {1., 0.}};
  return pts;
}

static double polylineValueAt(const QVector<QPointF>& pts, double x)
{
  if(x <= pts.front().x())
    return pts.front().y();
  for(int i = 1; i < pts.size(); ++i)
  {
    if(x <= pts[i].x())
    {
      const double x0 = pts[i - 1].x(), y0 = pts[i - 1].y();
      const double x1 = pts[i].x(), y1 = pts[i].y();
      if(x1 - x0 < 1e-9)
        return y1;
      return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
    }
  }
  return pts.back().y();
}

static void polylineToCurve(Curve::Model& curve, const QVector<QPointF>& pts)
{
  curve.clear();
  int64_t idx = 0;
  Curve::LinearSegment* prev{};
  for(int i = 1; i < pts.size(); ++i)
  {
    if(pts[i].x() - pts[i - 1].x() < 1e-9)
      continue;
    auto seg = new Curve::LinearSegment(Id<Curve::SegmentModel>(idx), &curve);
    seg->setStart({pts[i - 1].x(), pts[i - 1].y()});
    seg->setEnd({pts[i].x(), pts[i].y()});
    if(prev)
    {
      prev->setFollowing(seg->id());
      seg->setPrevious(prev->id());
    }
    curve.addSegment(seg);
    prev = seg;
    ++idx;
  }
  if(idx == 0)
    initFlatCurve(curve, pts.empty() ? 0. : pts.front().y());
}

// Restrict a polyline to [a, b] and rescale that span to [0, 1].
static QVector<QPointF>
polylineSlice(const QVector<QPointF>& pts, double a, double b)
{
  QVector<QPointF> out;
  const double span = b - a;
  if(span < 1e-9)
    return {{0., polylineValueAt(pts, a)}, {1., polylineValueAt(pts, a)}};

  out.push_back({0., polylineValueAt(pts, a)});
  for(const auto& p : pts)
  {
    if(p.x() > a + 1e-9 && p.x() < b - 1e-9)
      out.push_back({(p.x() - a) / span, p.y()});
  }
  out.push_back({1., polylineValueAt(pts, b)});
  return out;
}

// Compute the range (min/max) and the current value for an address by querying
// the device explorer. Returns std::nullopt if the address is unknown — the
// caller should fall back to a sensible default. Unlike Curve::CurveDomain,
// `value` is NOT clamped into the declared domain: unclipped parameters can
// legitimately sit outside it, and the sequence widens its range to match.
struct ResolvedDomain
{
  double min;
  double max;
  double value; // current device value, unclamped
  double normY; // (value - min) / (max - min), clamped to [0,1] for curve use
};

static std::optional<ResolvedDomain>
resolveDomain(const score::DocumentContext& ctx, const State::AddressAccessor& addr)
{
  auto* devPlugin = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devPlugin)
    return std::nullopt;

  auto* node = Device::try_getNodeFromAddress(devPlugin->rootNode(), addr.address);
  if(!node || !node->is<Device::AddressSettings>())
    return std::nullopt;

  const auto& settings = node->get<Device::AddressSettings>();

  // Member of a vector parameter: extract the addressed component.
  ossia::value val = settings.value;
  const auto& accs = addr.qualifiers.get().accessors;
  if(!accs.empty())
  {
    if(auto sub = ossia::get_value_at_index(val, accs); sub.valid())
      val = sub;
  }

  Curve::CurveDomain dom{settings.domain.get(), val};
  // Guarantee a non-degenerate domain so the normalization below is finite.
  if(dom.max - dom.min < 1e-9)
    dom.max = dom.min + 1.0;

  const double value = State::convert::value<double>(val);

  double normY = (value - dom.min) / (dom.max - dom.min);
  if(normY < 0.0)
    normY = 0.0;
  if(normY > 1.0)
    normY = 1.0;
  return ResolvedDomain{dom.min, dom.max, value, normY};
}

// Fixed-size vector member count (vec2f/3f/4f); nullopt for anything else.
static std::optional<int> vecSize(const ossia::value& v)
{
  switch(v.get_type())
  {
    case ossia::val_type::VEC2F:
      return 2;
    case ossia::val_type::VEC3F:
      return 3;
    case ossia::val_type::VEC4F:
      return 4;
    default:
      return std::nullopt;
  }
}

// Raw current device value of an address, if the node exists.
static std::optional<ossia::value> deviceRawValue(
    const score::DocumentContext& ctx, const State::AddressAccessor& addr)
{
  auto* devPlugin = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devPlugin)
    return std::nullopt;
  auto* node = Device::try_getNodeFromAddress(devPlugin->rootNode(), addr.address);
  if(!node || !node->is<Device::AddressSettings>())
    return std::nullopt;
  return node->get<Device::AddressSettings>().value;
}

// Current device value of a color parameter, both as QColor and as the raw
// ossia value (the latter is what gets recorded on IS states).
struct ResolvedColor
{
  QColor color;
  ossia::value raw;
};

static std::optional<ResolvedColor> resolveDeviceColor(
    const score::DocumentContext& ctx, const State::AddressAccessor& addr,
    const ossia::color_u& u)
{
  auto* devPlugin = ctx.findPlugin<Explorer::DeviceDocumentPlugin>();
  if(!devPlugin)
    return std::nullopt;

  auto* node = Device::try_getNodeFromAddress(devPlugin->rootNode(), addr.address);
  if(!node || !node->is<Device::AddressSettings>())
    return std::nullopt;

  const auto& settings = node->get<Device::AddressSettings>();
  return ResolvedColor{valueToColor(settings.value, u), settings.value};
}

// Helper: create a TimeSync + Event + State chain at a given date
// Returns the IDs of the created objects
struct ISNode
{
  Id<Scenario::TimeSyncModel> tsId;
  Id<Scenario::EventModel> evId;
  Id<Scenario::StateModel> stId;
};

static ISNode createIS(
    const TimeVal& date, double ypos, const score::DocumentContext& ctx,
    SequenceModel& seq)
{
  ISNode node;
  node.tsId = getStrongId(seq.timeSyncs);
  node.evId = getStrongId(seq.events);
  node.stId = getStrongId(seq.states);

  auto ts = new Scenario::TimeSyncModel(node.tsId, date, &seq);
  seq.timeSyncs.add(ts);

  auto ev = new Scenario::EventModel(node.evId, node.tsId, date, &seq);
  seq.events.add(ev);
  ts->addEvent(node.evId);

  auto st = new Scenario::StateModel(node.stId, node.evId, ypos, ctx, &seq);
  seq.states.add(st);
  ev->addState(node.stId);

  return node;
}

// Helper: create an interval connecting two states
static Id<Scenario::IntervalModel> createSection(
    const Id<Scenario::StateModel>& startSt, const Id<Scenario::StateModel>& endSt,
    const score::DocumentContext& ctx, SequenceModel& seq)
{
  auto itvId = getStrongId(seq.intervals);
  auto& sst = seq.states.at(startSt);
  auto& est = seq.states.at(endSt);
  const auto& sev = seq.events.at(sst.eventId());
  const auto& eev = seq.events.at(est.eventId());

  auto itv = new Scenario::IntervalModel(itvId, 0.0, ctx, &seq);
  itv->setStartState(startSt);
  itv->setEndState(endSt);
  seq.intervals.add(itv);

  Scenario::SetNextInterval(sst, *itv);
  Scenario::SetPreviousInterval(est, *itv);

  auto dur = eev.date() - sev.date();
  itv->setStartDate(sev.date());
  Scenario::IntervalDurations::Algorithms::fixAllDurations(*itv, dur);

  return itvId;
}

SequenceModel::SequenceModel(
    const TimeVal& duration, const Id<Process::ProcessModel>& id,
    const score::DocumentContext& ctx, QObject* parent)
    : Process::ProcessModel{
        duration, id, Metadata<ObjectKey_k, SequenceModel>::get(), parent}
    , m_context{ctx}
{
  // Mirror user slot-layout actions across sections. Connected before any
  // section exists so deserialized sections are watched too.
  intervals.mutable_added.connect<&SequenceModel::watchSection>(this);

  // Create start boundary node at time 0.
  // Boundary IDs follow the Scenario convention (start = 0, end = 1) so that
  // ScenarioInterface helpers and execution components agree on which TS / Event /
  // State is the "structural" entry / exit of the sub-scenario.
  m_startTimeSyncId = Scenario::startId<Scenario::TimeSyncModel>();
  m_startEventId = Scenario::startId<Scenario::EventModel>();
  const auto startStId = Scenario::startId<Scenario::StateModel>();

  auto startTs = new Scenario::TimeSyncModel(m_startTimeSyncId, TimeVal::zero(), this);
  startTs->setStartPoint(true);
  timeSyncs.add(startTs);

  auto startEv
      = new Scenario::EventModel(m_startEventId, m_startTimeSyncId, TimeVal::zero(), this);
  events.add(startEv);
  startTs->addEvent(m_startEventId);

  auto startSt = new Scenario::StateModel(startStId, m_startEventId, 0.02, ctx, this);
  states.add(startSt);
  startEv->addState(startStId);

  // Create end boundary node at duration
  m_endTimeSyncId = Scenario::endId<Scenario::TimeSyncModel>();
  m_endEventId = Scenario::endId<Scenario::EventModel>();
  const auto endStId = Scenario::endId<Scenario::StateModel>();

  auto endTs = new Scenario::TimeSyncModel(m_endTimeSyncId, duration, this);
  timeSyncs.add(endTs);

  auto endEv = new Scenario::EventModel(m_endEventId, m_endTimeSyncId, duration, this);
  events.add(endEv);
  endTs->addEvent(m_endEventId);

  auto endSt = new Scenario::StateModel(endStId, m_endEventId, 0.02, ctx, this);
  states.add(endSt);
  endEv->addState(endStId);

  // Create one section interval from start to end
  createSection(startStId, endStId, ctx, *this);

  // Derive parent boundary state IDs (not serialized, always reconstructed)
  if(auto* itv = qobject_cast<Scenario::IntervalModel*>(parent))
  {
    m_parentStartStateId = itv->startState();
    m_parentEndStateId = itv->endState();
  }

  rebuildParamPorts();

  metadata().setInstanceName(*this);
}

SequenceModel::~SequenceModel()
{
  intervals.clear();
  states.clear();
  events.clear();
  timeSyncs.clear();
}

// ---- Ordered helpers ----

QVector<Id<Scenario::TimeSyncModel>> SequenceModel::orderedTimeSyncs() const
{
  // Sort by date
  QVector<std::pair<TimeVal, Id<Scenario::TimeSyncModel>>> pairs;
  pairs.reserve(static_cast<int>(timeSyncs.size()));
  for(const auto& ts : timeSyncs)
    pairs.push_back({ts.date(), ts.id()});
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  QVector<Id<Scenario::TimeSyncModel>> result;
  result.reserve(pairs.size());
  for(const auto& p : pairs)
    result.push_back(p.second);
  return result;
}

QVector<Id<Scenario::IntervalModel>> SequenceModel::orderedIntervals() const
{
  // Sort by start date
  QVector<std::pair<TimeVal, Id<Scenario::IntervalModel>>> pairs;
  pairs.reserve(static_cast<int>(intervals.size()));
  for(const auto& itv : intervals)
    pairs.push_back({itv.date(), itv.id()});
  std::sort(pairs.begin(), pairs.end(), [](const auto& a, const auto& b) {
    return a.first < b.first;
  });

  QVector<Id<Scenario::IntervalModel>> result;
  result.reserve(pairs.size());
  for(const auto& p : pairs)
    result.push_back(p.second);
  return result;
}

// ---- Internal helpers ----

Id<Scenario::StateModel> SequenceModel::stateForTimeSync(
    const Id<Scenario::TimeSyncModel>& tsId) const
{
  const auto& ts = timeSyncs.at(tsId);
  SCORE_ASSERT(!ts.events().empty());
  const auto& ev = events.at(ts.events().front());
  SCORE_ASSERT(!ev.states().empty());
  return ev.states().front();
}

std::optional<Id<Scenario::IntervalModel>> SequenceModel::intervalBefore(
    const Id<Scenario::TimeSyncModel>& tsId) const
{
  auto stId = stateForTimeSync(tsId);
  const auto& st = states.at(stId);
  if(st.previousInterval())
    return *st.previousInterval();
  return std::nullopt;
}

std::optional<Id<Scenario::IntervalModel>> SequenceModel::intervalAfter(
    const Id<Scenario::TimeSyncModel>& tsId) const
{
  auto stId = stateForTimeSync(tsId);
  const auto& st = states.at(stId);
  if(st.nextInterval())
    return *st.nextInterval();
  return std::nullopt;
}

// ---- Sequence-level ports ----

void SequenceModel::rebuildParamPorts(const std::vector<int32_t>& savedIds)
{
  m_outlets.clear();
  if(!m_audioOutlet)
    m_audioOutlet = std::make_unique<Process::AudioOutlet>(
        QStringLiteral("out"), Id<Process::Port>(0), this);
  m_outlets.push_back(m_audioOutlet.get());

  std::vector<std::unique_ptr<Process::ValueOutlet>> newPorts;
  newPorts.reserve(m_namespace.size());
  for(int i = 0; i < m_namespace.size(); i++)
  {
    const auto& addr = m_namespace[i];
    const QString label = addr.toString();

    // Keep the existing port for this parameter if there is one, so cables
    // stay attached when the namespace changes.
    auto it = ossia::find_if(
        m_paramOutlets, [&](auto& p) { return p && p->name() == label; });
    if(it != m_paramOutlets.end())
    {
      newPorts.push_back(std::move(*it));
    }
    else
    {
      Id<Process::Port> id;
      if(i < (int)savedIds.size())
      {
        id = Id<Process::Port>(savedIds[i]);
      }
      else
      {
        // Fresh unique id (0 is the audio outlet)
        int32_t next = 1;
        auto used = [&](int32_t v) {
          for(auto& p : newPorts)
            if(p->id().val() == v)
              return true;
          for(auto& p : m_paramOutlets)
            if(p && p->id().val() == v)
              return true;
          return false;
        };
        while(used(next))
          ++next;
        id = Id<Process::Port>(next);
      }
      auto port = std::make_unique<Process::ValueOutlet>(label, id, this);
      // Shown per slot row by the sequence presenter, not in the header
      port->displayHandledExplicitly = true;
      newPorts.push_back(std::move(port));
    }
  }
  m_paramOutlets = std::move(newPorts);
  for(auto& p : m_paramOutlets)
    m_outlets.push_back(p.get());

  outletsChanged();
}

// ---- Namespace management ----

void SequenceModel::addParameter(
    const State::AddressAccessor& addr, const ossia::value& /*currentVal*/)
{
  RackSyncGuard rackGuard{m_rackSync};
  if(m_namespace.contains(addr))
    return;

  // Fixed-size vector parameters expand into one automation per member,
  // addressed with an index accessor. Arbitrary (variable-length) arrays are
  // not handled.
  if(!colorUnit(addr) && addr.qualifiers.get().accessors.empty())
  {
    if(auto raw = deviceRawValue(m_context, addr))
    {
      if(raw->get_type() == ossia::val_type::LIST)
        return;
      if(auto n = vecSize(*raw))
      {
        for(int i = 0; i < *n; ++i)
        {
          auto sub = addr;
          auto& acc = sub.qualifiers.get().accessors;
          acc.clear();
          acc.push_back(i);
          addParameter(sub, {});
        }
        return;
      }
    }
  }

  m_namespace.append(addr);

  // Color parameters are automated with a Gradient process per section,
  // flat at the current device color.
  if(auto* cu = colorUnit(addr))
  {
    auto resolved = resolveDeviceColor(m_context, addr, *cu);
    const QColor c
        = resolved ? resolved->color : QColor::fromRgbF(0.5, 0.5, 0.5, 1.);

    for(auto& itv : intervals)
    {
      auto procId = getStrongId(itv.processes);
      auto* grad
          = new Gradient::ProcessModel(itv.duration.defaultDuration(), procId, &itv);
      grad->outlet->setAddress(addr);
      Gradient::ProcessModel::gradient_colors g;
      g[0.] = c;
      g[1.] = c;
      grad->setGradient(g);
      Scenario::AddProcess(itv, grad);
      hideInstancePorts(*grad);
      addProcessToSectionRack(itv, procId);
    }

    namespaceChanged();
    rebuildParamPorts();
    return;
  }

  // Look up the actual current value + domain in the device explorer.
  // If absent (offline device, address removed), fall back to a flat midpoint.
  // The range starts as the declared domain widened by the current value —
  // unclipped parameters can sit outside their domain.
  auto resolved = resolveDomain(m_context, addr);
  const double minVal = resolved ? std::min(resolved->min, resolved->value) : 0.0;
  const double maxVal = resolved ? std::max(resolved->max, resolved->value) : 1.0;
  const double normY = resolved ? normalizeScalar(resolved->value, minVal, maxVal) : 0.5;

  // Create a flat automation at the resolved value in every section.
  for(auto& itv : intervals)
  {
    auto autoId = getStrongId(itv.processes);
    auto* automation
        = new Automation::ProcessModel(itv.duration.defaultDuration(), autoId, &itv);
    automation->setAddress(addr);
    automation->setMin(minVal);
    automation->setMax(maxVal);
    initFlatCurve(automation->curve(), normY);
    Scenario::AddProcess(itv, automation);
    hideInstancePorts(*automation);
    // Add to the section's rack so it renders in the section presenter
    addProcessToSectionRack(itv, autoId);
  }

  namespaceChanged();
  rebuildParamPorts();
}

void SequenceModel::removeParameter(const State::AddressAccessor& addr)
{
  RackSyncGuard rackGuard{m_rackSync};
  // Removing a vector parameter by its base address removes every expanded
  // member entry (added with index accessors by addParameter).
  if(!m_namespace.contains(addr) && addr.qualifiers.get().accessors.empty())
  {
    QList<State::AddressAccessor> members;
    for(const auto& entry : m_namespace)
    {
      if(entry.address == addr.address)
        members.push_back(entry);
    }
    for(const auto& m : members)
      removeParameter(m);
    return;
  }

  m_namespace.removeAll(addr);

  // Remove automations / gradients for this address from all section intervals
  for(auto& itv : intervals)
  {
    for(const auto& proc : itv.processes)
    {
      if(auto* auto_proc = qobject_cast<const Automation::ProcessModel*>(&proc))
      {
        if(auto_proc->address() == addr)
        {
          Scenario::RemoveProcess(itv, proc.id());
          break;
        }
      }
      else if(auto* grad = qobject_cast<const Gradient::ProcessModel*>(&proc))
      {
        if(grad->address() == addr)
        {
          Scenario::RemoveProcess(itv, proc.id());
          break;
        }
      }
    }
  }

  namespaceChanged();
  rebuildParamPorts();
}

void SequenceModel::mergeNamespace(const QList<State::AddressAccessor>& addrs)
{
  for(const auto& addr : addrs)
  {
    if(!m_namespace.contains(addr))
      addParameter(addr, ossia::value{});
  }
}

// ---- Linear structure mutation ----

// Section helper that uses caller-supplied IDs. The IDs are passed through
// directly; no getStrongId() calls so that the same IDs survive across
// undo/redo cycles.
static Id<Scenario::IntervalModel> createSectionWithId(
    const Id<Scenario::IntervalModel>& itvId,
    const Id<Scenario::StateModel>& startSt, const Id<Scenario::StateModel>& endSt,
    const score::DocumentContext& ctx, SequenceModel& seq)
{
  auto& sst = seq.states.at(startSt);
  auto& est = seq.states.at(endSt);
  const auto& sev = seq.events.at(sst.eventId());
  const auto& eev = seq.events.at(est.eventId());

  auto itv = new Scenario::IntervalModel(itvId, 0.0, ctx, &seq);
  itv->setStartState(startSt);
  itv->setEndState(endSt);
  seq.intervals.add(itv);

  Scenario::SetNextInterval(sst, *itv);
  Scenario::SetPreviousInterval(est, *itv);

  auto dur = eev.date() - sev.date();
  itv->setStartDate(sev.date());
  Scenario::IntervalDurations::Algorithms::fixAllDurations(*itv, dur);

  return itvId;
}

SequenceModel::AppendedSection SequenceModel::appendSection(const TimeVal& duration)
{
  // Single source of truth: pre-allocate fresh IDs and delegate to the
  // caller-supplied-IDs variant. Keeps direct callers (tests, programmatic
  // scenarios) working without needing to know about ID stability rules.
  AppendedSection info;
  info.prevEndTimeSyncId = m_endTimeSyncId;
  info.prevEndEventId = m_endEventId;
  info.prevDuration = this->duration();
  info.newEndTimeSyncId = getStrongId(timeSyncs);
  info.newEndEventId = getStrongId(events);
  info.newEndStateId = getStrongId(states);
  info.newIntervalId = getStrongId(intervals);

  appendSectionWithIds(info, duration);
  return info;
}

void SequenceModel::appendSectionWithIds(
    const AppendedSection& info, const TimeVal& duration)
{
  RackSyncGuard rackGuard{m_rackSync};
  // The current end IS stays at its date, becoming an intermediate IS.
  // A new end IS is created at current_end_date + duration.
  const auto& endTs = timeSyncs.at(m_endTimeSyncId);
  const TimeVal oldEndDate = endTs.date();
  const TimeVal newEndDate = oldEndDate + duration;

  // Create new end IS using pre-allocated IDs
  auto newTs = new Scenario::TimeSyncModel(info.newEndTimeSyncId, newEndDate, this);
  timeSyncs.add(newTs);

  auto newEv
      = new Scenario::EventModel(info.newEndEventId, info.newEndTimeSyncId, newEndDate, this);
  events.add(newEv);
  newTs->addEvent(info.newEndEventId);

  auto newSt = new Scenario::StateModel(
      info.newEndStateId, info.newEndEventId, 0.02, m_context, this);
  states.add(newSt);
  newEv->addState(info.newEndStateId);

  // Create interval from old end boundary to new end boundary
  const auto oldEndStId = stateForTimeSync(m_endTimeSyncId);
  createSectionWithId(
      info.newIntervalId, oldEndStId, info.newEndStateId, m_context, *this);

  // Create automations in the new interval for all namespace parameters.
  // Each one ramps from the value carried by the previous end IS (which is
  // becoming an intermediate boundary and keeps its value) to the current
  // device value, which lands on the new end IS.
  auto& newItv = intervals.at(info.newIntervalId);
  auto& newEndSt = states.at(info.newEndStateId);
  auto& prevEndSt = states.at(stateForTimeSync(info.prevEndTimeSyncId));
  const auto prevEndMessages = Process::flatten(prevEndSt.messages().rootNode());
  for(const auto& addr : m_namespace)
  {
    // Color parameters: gradient from the previous end IS color to the
    // current device color.
    if(auto* cu = colorUnit(addr))
    {
      auto resolved = resolveDeviceColor(m_context, addr, *cu);
      const QColor devColor
          = resolved ? resolved->color : QColor::fromRgbF(0.5, 0.5, 0.5, 1.);

      QColor startColor = devColor;
      for(const auto& msg : prevEndMessages)
      {
        if(msg.address == addr)
        {
          startColor = valueToColor(msg.value, *cu);
          break;
        }
      }

      auto procId = getStrongId(newItv.processes);
      auto* grad = new Gradient::ProcessModel(
          newItv.duration.defaultDuration(), procId, &newItv);
      grad->outlet->setAddress(addr);
      Gradient::ProcessModel::gradient_colors g;
      g[0.] = startColor;
      g[1.] = devColor;
      grad->setGradient(g);
      Scenario::AddProcess(newItv, grad);
      hideInstancePorts(*grad);
      addProcessToSectionRack(newItv, procId);

      if(resolved)
      {
        State::MessageList msgs{State::Message{addr, resolved->raw}};
        Scenario::updateModelWithMessageList(newEndSt.messages(), std::move(msgs));
      }
      continue;
    }

    auto resolved = resolveDomain(m_context, addr);
    const double devVal = resolved ? resolved->value : 0.5;

    // Value at the section start = the value recorded on the previous end IS,
    // falling back to the current device value if none was recorded.
    double startVal = devVal;
    for(const auto& msg : prevEndMessages)
    {
      if(msg.address == addr)
      {
        startVal = State::convert::value<double>(msg.value);
        break;
      }
    }

    // Widen the existing automations' range first if either value falls
    // outside it, then adopt the (possibly widened) sequence-wide range for
    // the new section so all sections share the same normalization.
    if(resolved)
    {
      ensureParamRange(addr, startVal);
      ensureParamRange(addr, devVal);
    }
    double minVal, maxVal;
    if(!currentParamRange(addr, minVal, maxVal))
    {
      minVal = resolved ? std::min(resolved->min, std::min(startVal, devVal)) : 0.0;
      maxVal = resolved ? std::max(resolved->max, std::max(startVal, devVal)) : 1.0;
    }

    auto autoId = getStrongId(newItv.processes);
    auto* automation = new Automation::ProcessModel(
        newItv.duration.defaultDuration(), autoId, &newItv);
    automation->setAddress(addr);
    automation->setMin(minVal);
    automation->setMax(maxVal);
    if(resolved)
      initSegmentCurve(
          automation->curve(), normalizeScalar(startVal, minVal, maxVal),
          normalizeScalar(devVal, minVal, maxVal));
    else
      initFlatCurve(automation->curve(), 0.5);
    Scenario::AddProcess(newItv, automation);
    hideInstancePorts(*automation);

    // Add to the section's rack so it renders in the section presenter
    addProcessToSectionRack(newItv, autoId);

    // Record the current device value on the new end state
    if(resolved)
    {
      State::MessageList msgs{State::Message{addr, ossia::value{devVal}}};
      Scenario::updateModelWithMessageList(newEndSt.messages(), std::move(msgs));
    }
  }

  // Update end pointers
  m_endTimeSyncId = info.newEndTimeSyncId;
  m_endEventId = info.newEndEventId;

  // Update process duration
  setDuration(newEndDate);

  // The new section adopts the slot layout of the previous last section
  {
    const auto ord = orderedIntervals();
    if(ord.size() > 1)
      mirrorRackLayout(intervals.at(ord[ord.size() - 2]));
  }

  structureChanged();
}

void SequenceModel::undoAppendSection(const AppendedSection& info)
{
  RackSyncGuard rackGuard{m_rackSync};
  // Disconnect the prev-end state's next-interval link before removal
  auto& prevEndSt = states.at(stateForTimeSync(info.prevEndTimeSyncId));
  prevEndSt.setNextInterval(std::nullopt);

  // Remove the new state's previous-interval link too
  auto& newEndSt = states.at(info.newEndStateId);
  newEndSt.setPreviousInterval(std::nullopt);

  // Remove the interval connecting prev-end to new-end
  intervals.remove(info.newIntervalId);

  // Remove the new-end entities
  states.remove(info.newEndStateId);
  events.remove(info.newEndEventId);
  timeSyncs.remove(info.newEndTimeSyncId);

  // Restore end pointers
  m_endTimeSyncId = info.prevEndTimeSyncId;
  m_endEventId = info.prevEndEventId;

  setDuration(info.prevDuration);
  structureChanged();
}

void SequenceModel::moveIS(
    const Id<Scenario::TimeSyncModel>& tsId, const TimeVal& newDate)
{
  // Don't move start or end
  SCORE_ASSERT(tsId != m_startTimeSyncId);
  SCORE_ASSERT(tsId != m_endTimeSyncId);

  auto& ts = timeSyncs.at(tsId);
  const TimeVal oldDate = ts.date();
  ts.setDate(newDate);

  const auto stId = stateForTimeSync(tsId);
  auto& ev = events.at(states.at(stId).eventId());
  ev.setDate(newDate);

  // Resize left-adjacent interval
  if(auto leftItvId = intervalBefore(tsId))
  {
    auto& leftItv = intervals.at(*leftItvId);
    const auto& startEv
        = events.at(states.at(leftItv.startState()).eventId());
    auto newDur = newDate - startEv.date();
    Scenario::IntervalDurations::Algorithms::fixAllDurations(leftItv, newDur);
    for(auto& proc : leftItv.processes)
      proc.setParentDuration(ExpandMode::Scale, newDur);
  }

  // Resize right-adjacent interval
  if(auto rightItvId = intervalAfter(tsId))
  {
    auto& rightItv = intervals.at(*rightItvId);
    const auto& endEv = events.at(states.at(rightItv.endState()).eventId());
    auto newDur = endEv.date() - newDate;
    rightItv.setStartDate(newDate);
    Scenario::IntervalDurations::Algorithms::fixAllDurations(rightItv, newDur);
    for(auto& proc : rightItv.processes)
      proc.setParentDuration(ExpandMode::Scale, newDur);
  }

  structureChanged();
}

void SequenceModel::moveISRipple(
    const Id<Scenario::TimeSyncModel>& tsId, const TimeVal& newDate)
{
  SCORE_ASSERT(tsId != m_startTimeSyncId);

  auto& ts = timeSyncs.at(tsId);
  const TimeVal oldDate = ts.date();
  const TimeVal delta = newDate - oldDate;
  if(delta == TimeVal::zero())
    return;

  const auto leftItvId = intervalBefore(tsId);

  // Shift this IS and everything at or after it
  for(auto& t : timeSyncs)
  {
    if(t.date() >= oldDate)
      t.setDate(t.date() + delta);
  }
  for(auto& ev : events)
  {
    if(ev.date() >= oldDate)
      ev.setDate(ev.date() + delta);
  }

  // Translate the intervals that start at or after the moved IS; their
  // durations are preserved.
  for(auto& itv : intervals)
  {
    if(leftItvId && itv.id() == *leftItvId)
      continue;
    if(itv.date() >= oldDate)
      itv.setStartDate(itv.date() + delta);
  }

  // Resize the left-adjacent interval; its processes scale with it.
  if(leftItvId)
  {
    auto& leftItv = intervals.at(*leftItvId);
    const auto& startEv = events.at(states.at(leftItv.startState()).eventId());
    auto newDur = newDate - startEv.date();
    Scenario::IntervalDurations::Algorithms::fixAllDurations(leftItv, newDur);
    for(auto& proc : leftItv.processes)
      proc.setParentDuration(ExpandMode::Scale, newDur);
  }

  setDuration(duration() + delta);
  structureChanged();
}

std::optional<Id<Scenario::IntervalModel>>
SequenceModel::sectionAt(const TimeVal& date) const
{
  const auto eps = TimeVal::fromMsecs(1);
  for(const auto& itv : intervals)
  {
    if(itv.date() + eps < date
       && date < itv.date() + itv.duration.defaultDuration() - eps)
      return itv.id();
  }
  return std::nullopt;
}

void SequenceModel::insertISWithIds(const InsertedIS& info, const TimeVal& date)
{
  RackSyncGuard rackGuard{m_rackSync};
  auto& leftItv = intervals.at(info.leftItvId);
  const TimeVal leftStart = leftItv.date();
  const TimeVal oldDur = leftItv.duration.defaultDuration();
  const double t = (date - leftStart) / oldDur;

  const auto oldEndStId = leftItv.endState();
  auto& oldEndSt = states.at(oldEndStId);

  // Create the new IS at `date`
  auto newTs = new Scenario::TimeSyncModel(info.newTsId, date, this);
  timeSyncs.add(newTs);
  auto newEv = new Scenario::EventModel(info.newEvId, info.newTsId, date, this);
  events.add(newEv);
  newTs->addEvent(info.newEvId);
  auto newSt
      = new Scenario::StateModel(info.newStId, info.newEvId, 0.02, m_context, this);
  states.add(newSt);
  newEv->addState(info.newStId);

  // Rewire: the left interval now ends at the new IS. Its processes are no
  // longer "before" the old end state.
  for(auto& proc : leftItv.processes)
    Scenario::RemoveProcessBeforeState(oldEndSt, proc);
  oldEndSt.setPreviousInterval(std::nullopt);
  leftItv.setEndState(info.newStId);
  Scenario::SetPreviousInterval(*newSt, leftItv);

  // Create the right interval from the new IS to the old section end
  createSectionWithId(info.rightItvId, info.newStId, oldEndStId, m_context, *this);
  auto& rightItv = intervals.at(info.rightItvId);

  // Split every automation curve at t, record the value on the new IS state.
  // Iterate the namespace so the right section's slot order matches every
  // other section — otherwise the rows don't line up across the boundary.
  for(const auto& addr : m_namespace)
  {
    // Color parameters: split the gradient at t
    if(auto* cu = colorUnit(addr))
    {
      auto* leftGrad = gradientForAddr(leftItv, addr);
      if(!leftGrad)
        continue;

      const auto stops = leftGrad->gradient();
      const QColor cAt = gradientValueAt(stops, t);

      Gradient::ProcessModel::gradient_colors leftStops, rightStops;
      for(const auto& [pos, col] : stops)
      {
        if(pos < t)
          leftStops[pos / t] = col;
        else if(pos > t)
          rightStops[(pos - t) / (1. - t)] = col;
      }
      leftStops[1.] = cAt;
      rightStops[0.] = cAt;

      auto procId = getStrongId(rightItv.processes);
      auto* rightGrad = new Gradient::ProcessModel(
          rightItv.duration.defaultDuration(), procId, &rightItv);
      rightGrad->outlet->setAddress(addr);
      rightGrad->setGradient(rightStops);
      Scenario::AddProcess(rightItv, rightGrad);
      hideInstancePorts(*rightGrad);
      addProcessToSectionRack(rightItv, procId);

      leftGrad->setGradient(leftStops);

      State::MessageList msgs{State::Message{addr, colorToValue(cAt, *cu)}};
      Scenario::updateModelWithMessageList(newSt->messages(), std::move(msgs));
      continue;
    }

    auto* leftAuto = automationForAddr(leftItv, addr);
    if(!leftAuto)
      continue;

    const auto pts = curveToPolyline(leftAuto->curve());
    const double vNorm = polylineValueAt(pts, t);
    const double vReal
        = leftAuto->min() + vNorm * (leftAuto->max() - leftAuto->min());

    // Right part
    auto autoId = getStrongId(rightItv.processes);
    auto* rightAuto = new Automation::ProcessModel(
        rightItv.duration.defaultDuration(), autoId, &rightItv);
    rightAuto->setAddress(leftAuto->address());
    rightAuto->setMin(leftAuto->min());
    rightAuto->setMax(leftAuto->max());
    polylineToCurve(rightAuto->curve(), polylineSlice(pts, t, 1.));
    Scenario::AddProcess(rightItv, rightAuto);
    hideInstancePorts(*rightAuto);
    Scenario::Slot slot{{autoId}, autoId, kSectionSlotHeight};
    rightItv.addSlot(std::move(slot));
    rightItv.setSmallViewVisible(true);

    // Left part
    polylineToCurve(leftAuto->curve(), polylineSlice(pts, 0., t));

    // Record the value on the new IS state
    State::MessageList msgs{
        State::Message{leftAuto->address(), ossia::value{vReal}}};
    Scenario::updateModelWithMessageList(newSt->messages(), std::move(msgs));
  }

  // Shorten the left interval. Scale mode: the curves were already re-sliced
  // to the new span, they must not be x-rescaled again.
  const TimeVal newLeftDur = date - leftStart;
  Scenario::IntervalDurations::Algorithms::fixAllDurations(leftItv, newLeftDur);
  for(auto& proc : leftItv.processes)
    proc.setParentDuration(ExpandMode::Scale, newLeftDur);

  // The new right section adopts the split section's slot layout
  mirrorRackLayout(leftItv);

  structureChanged();
}

void SequenceModel::undoInsertIS(const InsertedIS& info)
{
  RackSyncGuard rackGuard{m_rackSync};
  auto& leftItv = intervals.at(info.leftItvId);
  auto& rightItv = intervals.at(info.rightItvId);
  const auto endStId = rightItv.endState();
  auto& endSt = states.at(endStId);

  const TimeVal leftDur = leftItv.duration.defaultDuration();
  const TimeVal rightDur = rightItv.duration.defaultDuration();
  const TimeVal mergedDur = leftDur + rightDur;
  const double t = leftDur / mergedDur;

  // Merge the split automation / gradient curves back
  for(auto& proc : leftItv.processes)
  {
    if(auto* leftGrad = qobject_cast<Gradient::ProcessModel*>(&proc))
    {
      auto* rightGrad = gradientForAddr(rightItv, leftGrad->address());
      if(!rightGrad)
        continue;

      // The stops at the junction (left pos 1, right pos 0) were inserted
      // artificially by the split — drop them so undo restores the original
      // gradient without a leftover point.
      Gradient::ProcessModel::gradient_colors merged;
      for(const auto& [pos, col] : leftGrad->gradient())
      {
        if(pos < 1. - 1e-9)
          merged[pos * t] = col;
      }
      for(const auto& [pos, col] : rightGrad->gradient())
      {
        if(pos > 1e-9)
          merged[t + pos * (1. - t)] = col;
      }
      leftGrad->setGradient(merged);
      continue;
    }

    auto* leftAuto = qobject_cast<Automation::ProcessModel*>(&proc);
    if(!leftAuto)
      continue;
    auto* rightAuto = automationForAddr(rightItv, leftAuto->address());
    if(!rightAuto)
      continue;

    // Drop the junction point at t (last left point / first right point):
    // it was inserted artificially by the split, and the surrounding points
    // are collinear through it, so removal restores the original curve.
    QVector<QPointF> merged;
    const auto leftPts = curveToPolyline(leftAuto->curve());
    for(int i = 0; i < leftPts.size() - 1; ++i)
      merged.push_back({leftPts[i].x() * t, leftPts[i].y()});
    const auto rightPts = curveToPolyline(rightAuto->curve());
    for(int i = 1; i < rightPts.size(); ++i)
    {
      const double x = t + rightPts[i].x() * (1. - t);
      if(merged.empty() || x > merged.back().x() + 1e-9)
        merged.push_back({x, rightPts[i].y()});
    }
    polylineToCurve(leftAuto->curve(), merged);
  }

  // Rewire the left interval back to the old section end. The right interval's
  // processes must be unregistered from the surviving end state first.
  for(auto& proc : rightItv.processes)
    Scenario::RemoveProcessBeforeState(endSt, proc);
  auto& newSt = states.at(info.newStId);
  newSt.setPreviousInterval(std::nullopt);
  newSt.setNextInterval(std::nullopt);
  endSt.setPreviousInterval(std::nullopt);
  leftItv.setEndState(endStId);
  Scenario::SetPreviousInterval(endSt, leftItv);

  // Remove the right interval and the inserted IS entities
  intervals.remove(info.rightItvId);
  states.remove(info.newStId);
  events.remove(info.newEvId);
  timeSyncs.remove(info.newTsId);

  // Restore the merged duration. Scale mode: the curves were already merged
  // back to the full span, they must not be x-rescaled again.
  Scenario::IntervalDurations::Algorithms::fixAllDurations(leftItv, mergedDur);
  for(auto& proc : leftItv.processes)
    proc.setParentDuration(ExpandMode::Scale, mergedDur);

  structureChanged();
}

// ---- IS value management ----

void SequenceModel::watchSection(Scenario::IntervalModel& itv)
{
  const auto sync = [this, &itv] {
    if(!m_rackSync)
      mirrorRackLayout(itv);
  };
  connect(
      &itv, &Scenario::IntervalModel::slotResized, this,
      [sync](Scenario::SlotId s) {
    if(s.smallView())
      sync();
      });
  connect(
      &itv, &Scenario::IntervalModel::slotAdded, this,
      [sync](Scenario::SlotId s) {
    if(s.smallView())
      sync();
      });
  connect(
      &itv, &Scenario::IntervalModel::slotRemoved, this,
      [sync](Scenario::SlotId s) {
    if(s.smallView())
      sync();
      });
  connect(
      &itv, &Scenario::IntervalModel::slotsSwapped, this,
      [sync](int, int, Scenario::Slot::RackView v) {
    if(v == Scenario::Slot::SmallView)
      sync();
      });
  connect(
      &itv, &Scenario::IntervalModel::layerAdded, this,
      [sync](Scenario::SlotId s, const Id<Process::ProcessModel>&) {
    if(s.smallView())
      sync();
      });
  connect(
      &itv, &Scenario::IntervalModel::layerRemoved, this,
      [sync](Scenario::SlotId s, const Id<Process::ProcessModel>&) {
    if(s.smallView())
      sync();
      });
  connect(
      &itv, &Scenario::IntervalModel::frontLayerChanged, this,
      [sync](int, const OptionalId<Process::ProcessModel>&) { sync(); });
}

std::optional<Id<Process::ProcessModel>> SequenceModel::correspondingProcess(
    const Scenario::IntervalModel& source, const Id<Process::ProcessModel>& pid,
    Scenario::IntervalModel& target) const
{
  auto it = source.processes.find(pid);
  if(it == source.processes.end())
    return std::nullopt;

  if(auto* a = qobject_cast<const Automation::ProcessModel*>(&*it))
  {
    if(auto* t = automationForAddr(target, a->address()))
      return t->id();
  }
  else if(auto* g = qobject_cast<const Gradient::ProcessModel*>(&*it))
  {
    if(auto* t = gradientForAddr(target, g->address()))
      return t->id();
  }
  return std::nullopt;
}

void SequenceModel::mirrorRackLayout(const Scenario::IntervalModel& source)
{
  RackSyncGuard guard{m_rackSync};

  for(auto& other_c : intervals)
  {
    auto& other = const_cast<Scenario::IntervalModel&>(other_c);
    if(&other == &source)
      continue;

    Scenario::Rack rack;
    std::vector<Id<Process::ProcessModel>> used;
    for(const auto& slot : source.smallView())
    {
      Scenario::Slot s;
      s.height = slot.height;
      s.nodal = slot.nodal;
      for(const auto& pid : slot.processes)
      {
        if(auto mapped = correspondingProcess(source, pid, other))
        {
          s.processes.push_back(*mapped);
          used.push_back(*mapped);
        }
      }
      if(s.processes.empty())
        continue;
      if(slot.frontProcess)
      {
        if(auto mapped = correspondingProcess(source, *slot.frontProcess, other))
          s.frontProcess = *mapped;
      }
      if(!s.frontProcess || !ossia::contains(s.processes, *s.frontProcess))
        s.frontProcess = s.processes.front();
      rack.push_back(std::move(s));
    }

    // Processes with no counterpart in the source keep their own slot so they
    // never silently disappear from view.
    for(auto& proc : other.processes)
    {
      if(!ossia::contains(used, proc.id()))
        rack.push_back(Scenario::Slot{{proc.id()}, proc.id(), kSectionSlotHeight});
    }

    other.replaceSmallView(rack);
  }
}

Automation::ProcessModel* SequenceModel::automationForAddr(
    Scenario::IntervalModel& itv, const State::AddressAccessor& addr) const
{
  for(auto& proc : itv.processes)
  {
    if(auto* auto_proc = qobject_cast<Automation::ProcessModel*>(&proc))
    {
      if(auto_proc->address() == addr)
        return auto_proc;
    }
  }
  return nullptr;
}

Gradient::ProcessModel* SequenceModel::gradientForAddr(
    Scenario::IntervalModel& itv, const State::AddressAccessor& addr) const
{
  for(auto& proc : itv.processes)
  {
    if(auto* grad = qobject_cast<Gradient::ProcessModel*>(&proc))
    {
      if(grad->address() == addr)
        return grad;
    }
  }
  return nullptr;
}

bool SequenceModel::currentParamRange(
    const State::AddressAccessor& addr, double& min, double& max) const
{
  for(auto& itv : intervals)
  {
    if(auto* ap
       = automationForAddr(const_cast<Scenario::IntervalModel&>(itv), addr))
    {
      min = ap->min();
      max = ap->max();
      return true;
    }
  }
  return false;
}

void SequenceModel::ensureParamRange(const State::AddressAccessor& addr, double v)
{
  double curMin{}, curMax{};
  if(!currentParamRange(addr, curMin, curMax))
    return;

  if(v >= curMin && v <= curMax)
    return;

  const double newMin = std::min(curMin, v);
  const double newMax = std::max(curMax, v);
  const double oldRange = curMax - curMin;
  const double newRange = newMax - newMin;
  if(oldRange < 1e-9 || newRange < 1e-9)
    return;

  // Renormalize every automation curve for this address so the curves keep
  // denoting the same actual values within the widened range.
  for(auto& itv : intervals)
  {
    auto* ap = automationForAddr(const_cast<Scenario::IntervalModel&>(itv), addr);
    if(!ap)
      continue;

    auto& curve = ap->curve();
    for(auto* seg : curve.sortedSegments())
    {
      const auto s = seg->start();
      const auto e = seg->end();
      seg->setStart({s.x(), ((s.y() * oldRange + curMin) - newMin) / newRange});
      seg->setEnd({e.x(), ((e.y() * oldRange + curMin) - newMin) / newRange});
    }
    curve.changed();
    ap->setMin(newMin);
    ap->setMax(newMax);
  }
}

void SequenceModel::syncAutomationEndpoints(
    const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
    const ossia::value& val)
{
  // Color parameters: pull the value into the adjacent gradients' boundary stops.
  if(auto* cu = colorUnit(addr))
  {
    const QColor c = valueToColor(val, *cu);
    if(auto leftItvId = intervalBefore(tsId))
    {
      if(auto* grad = gradientForAddr(intervals.at(*leftItvId), addr))
      {
        auto g = grad->gradient();
        g[1.] = c;
        grad->setGradient(g);
      }
    }
    if(auto rightItvId = intervalAfter(tsId))
    {
      if(auto* grad = gradientForAddr(intervals.at(*rightItvId), addr))
      {
        auto g = grad->gradient();
        g[0.] = c;
        grad->setGradient(g);
      }
    }
    return;
  }

  const double v = State::convert::value<double>(val);

  // Out-of-range values widen the sequence-wide range (unclipped parameters).
  ensureParamRange(addr, v);

  // Update end point of left-adjacent automation (curve x=1)
  if(auto leftItvId = intervalBefore(tsId))
  {
    if(auto* ap = automationForAddr(intervals.at(*leftItvId), addr))
    {
      auto& curve = ap->curve();
      auto segs = curve.sortedSegments();
      if(!segs.empty())
      {
        segs.back()->setEnd({1.0, normalizeScalar(v, ap->min(), ap->max())});
        curve.changed();
      }
    }
  }

  // Update start point of right-adjacent automation (curve x=0)
  if(auto rightItvId = intervalAfter(tsId))
  {
    if(auto* ap = automationForAddr(intervals.at(*rightItvId), addr))
    {
      auto& curve = ap->curve();
      auto segs = curve.sortedSegments();
      if(!segs.empty())
      {
        segs.front()->setStart({0.0, normalizeScalar(v, ap->min(), ap->max())});
        curve.changed();
      }
    }
  }
}

void SequenceModel::setISValue(
    const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
    const ossia::value& val)
{
  // 1. Persist the user-supplied value on the IS state as a flat message.
  //    The MessageItemModel is what the inspector and the executor read.
  if(auto* state = findState(stateForTimeSync(tsId)))
  {
    State::MessageList lst{State::Message{addr, val}};
    Scenario::updateModelWithMessageList(state->messages(), std::move(lst));
  }

  // 2. Pull the new value into the adjacent automations' endpoints.
  syncAutomationEndpoints(tsId, addr, val);

  // 3. Freeze propagation: walk forward through consecutive frozen IS so that
  //    each one stores the same value and its automations stay flat.
  auto ordered = orderedTimeSyncs();
  int tsIdx = ordered.indexOf(tsId);
  if(tsIdx < 0)
    return;

  for(int i = tsIdx + 1; i < ordered.size(); ++i)
  {
    const auto& nextTsId = ordered[i];
    if(!isParamFrozen(nextTsId, addr))
      break;
    if(auto* state = findState(stateForTimeSync(nextTsId)))
    {
      State::MessageList lst{State::Message{addr, val}};
      Scenario::updateModelWithMessageList(state->messages(), std::move(lst));
    }
    syncAutomationEndpoints(nextTsId, addr, val);
  }
}

void SequenceModel::freezeISParam(
    const Id<Scenario::TimeSyncModel>& tsId, const State::AddressAccessor& addr,
    bool frozen)
{
  if(frozen)
    m_frozenParams[tsId].insert(addr);
  else
    m_frozenParams[tsId].remove(addr);
}

bool SequenceModel::isParamFrozen(
    const Id<Scenario::TimeSyncModel>& tsId,
    const State::AddressAccessor& addr) const
{
  auto it = m_frozenParams.find(tsId);
  if(it == m_frozenParams.end())
    return false;
  return it->contains(addr);
}

void SequenceModel::rebuildAutomations(const State::AddressAccessor& addr)
{
  RackSyncGuard rackGuard{m_rackSync};
  if(auto* cu = colorUnit(addr))
  {
    auto resolved = resolveDeviceColor(m_context, addr, *cu);
    const QColor c
        = resolved ? resolved->color : QColor::fromRgbF(0.5, 0.5, 0.5, 1.);
    for(auto& itv : intervals)
    {
      if(gradientForAddr(const_cast<Scenario::IntervalModel&>(itv), addr))
        continue;
      auto procId = getStrongId(itv.processes);
      auto* grad
          = new Gradient::ProcessModel(itv.duration.defaultDuration(), procId, &itv);
      grad->outlet->setAddress(addr);
      Gradient::ProcessModel::gradient_colors g;
      g[0.] = c;
      g[1.] = c;
      grad->setGradient(g);
      Scenario::AddProcess(itv, grad);
      hideInstancePorts(*grad);
      addProcessToSectionRack(itv, procId);
    }
    return;
  }

  for(auto& itv : intervals)
  {
    // Check if an automation for addr already exists
    bool found = false;
    for(const auto& proc : itv.processes)
    {
      if(auto* auto_proc = qobject_cast<const Automation::ProcessModel*>(&proc))
      {
        if(auto_proc->address() == addr)
        {
          found = true;
          break;
        }
      }
    }

    if(!found)
    {
      auto resolved = resolveDomain(m_context, addr);
      // Prefer the sequence-wide range from sibling sections, so every
      // section shares the same normalization.
      double minVal, maxVal;
      if(!currentParamRange(addr, minVal, maxVal))
      {
        minVal = resolved ? std::min(resolved->min, resolved->value) : 0.0;
        maxVal = resolved ? std::max(resolved->max, resolved->value) : 1.0;
      }
      const double normY
          = resolved ? normalizeScalar(resolved->value, minVal, maxVal) : 0.5;

      auto autoId = getStrongId(itv.processes);
      auto* automation = new Automation::ProcessModel(
          itv.duration.defaultDuration(), autoId, &itv);
      automation->setAddress(addr);
      automation->setMin(minVal);
      automation->setMax(maxVal);
      initFlatCurve(automation->curve(), normY);
      Scenario::AddProcess(itv, automation);
    hideInstancePorts(*automation);
      addProcessToSectionRack(itv, autoId);
    }
  }
}

void SequenceModel::repairSectionRacks()
{
  RackSyncGuard rackGuard{m_rackSync};
  // Files saved before slots were added to section intervals have automations
  // that are in no small-view slot, so nothing renders in the section
  // presenters. Give every slot-less process its own slot.
  for(auto& itv : intervals)
  {
    for(const auto& proc : itv.processes)
    {
      hideInstancePorts(const_cast<Process::ProcessModel&>(proc));
      bool inSlot = false;
      for(const auto& slot : itv.smallView())
      {
        if(ossia::contains(slot.processes, proc.id()))
        {
          inSlot = true;
          break;
        }
      }
      if(!inSlot)
        addProcessToSectionRack(itv, proc.id());
    }
    if(!itv.smallView().empty())
      itv.setSmallViewVisible(true);
  }
}

// ---- ProcessModel overrides ----

TimeVal SequenceModel::contentDuration() const noexcept
{
  TimeVal max = TimeVal::zero();
  for(const auto& ts : timeSyncs)
  {
    if(ts.date() > max)
      max = ts.date();
  }
  return max;
}

void SequenceModel::setDurationAndScale(const TimeVal& newDuration) noexcept
{
  if(duration() == TimeVal::zero())
    return;

  double scale = newDuration / duration();

  for(auto& ts : timeSyncs)
    ts.setDate(ts.date() * scale);

  for(auto& ev : events)
    ev.setDate(ev.date() * scale);

  for(auto& itv : intervals)
  {
    itv.setStartDate(itv.date() * scale);
    auto newdur = itv.duration.defaultDuration() * scale;
    Scenario::IntervalDurations::Algorithms::scaleAllDurations(itv, newdur);
    for(auto& proc : itv.processes)
      proc.setParentDuration(ExpandMode::Scale, newdur);
  }

  setDuration(newDuration);
}

// Last-section-absorbs-slack strategy when the parent interval grows or shrinks
// in non-Scale mode. Mirrors how Nodal::Model defers to its child processes
// instead of silently keeping their old extents.
static Scenario::IntervalModel*
findLastSectionInterval(const score::EntityMap<Scenario::IntervalModel>& intervals)
{
  Scenario::IntervalModel* last = nullptr;
  TimeVal lastDate = TimeVal::zero();
  for(auto& itv : intervals)
  {
    if(!last || itv.date() >= lastDate)
    {
      lastDate = itv.date();
      last = &const_cast<Scenario::IntervalModel&>(itv);
    }
  }
  return last;
}

void SequenceModel::setDurationAndGrow(const TimeVal& newDuration) noexcept
{
  const auto cur = duration();
  if(newDuration <= cur)
  {
    setDuration(newDuration);
    return;
  }

  auto* lastItv = findLastSectionInterval(intervals);
  if(!lastItv)
  {
    setDuration(newDuration);
    return;
  }

  const auto extra = newDuration - cur;

  // Move end boundary forward by the extra delta
  timeSyncs.at(m_endTimeSyncId).setDate(timeSyncs.at(m_endTimeSyncId).date() + extra);
  events.at(m_endEventId).setDate(events.at(m_endEventId).date() + extra);
  states.at(stateForTimeSync(m_endTimeSyncId)); // touch — keep symmetry; state has no date

  // Stretch the last section. Its processes scale with it: section curves are
  // normalized between the surrounding ISes (endpoints pinned to IS values),
  // so GrowShrink would squash them into the left of the grown section.
  const auto newSecDur = lastItv->duration.defaultDuration() + extra;
  Scenario::IntervalDurations::Algorithms::fixAllDurations(*lastItv, newSecDur);
  for(auto& proc : lastItv->processes)
    proc.setParentDuration(ExpandMode::Scale, newSecDur);

  setDuration(newDuration);
}

void SequenceModel::setDurationAndShrink(const TimeVal& newDuration) noexcept
{
  const auto cur = duration();
  if(newDuration >= cur)
  {
    setDuration(newDuration);
    return;
  }

  auto* lastItv = findLastSectionInterval(intervals);
  if(!lastItv)
  {
    setDuration(newDuration);
    return;
  }

  const auto delta = cur - newDuration;
  const auto curSecDur = lastItv->duration.defaultDuration();
  // Clamp the last section to a strictly-positive minimum so the executor
  // does not see a zero-length interval. If the request would underflow
  // (the sum of fixed earlier sections already exceeds newDuration) we
  // clamp the last section and propagate the clamped delta upwards.
  const auto minSecDur = TimeVal::fromMsecs(1.);
  auto newSecDur = curSecDur - delta;
  if(newSecDur < minSecDur)
    newSecDur = minSecDur;
  const auto actualDelta = curSecDur - newSecDur;
  const auto actualDuration = cur - actualDelta;

  timeSyncs.at(m_endTimeSyncId).setDate(timeSyncs.at(m_endTimeSyncId).date() - actualDelta);
  events.at(m_endEventId).setDate(events.at(m_endEventId).date() - actualDelta);

  Scenario::IntervalDurations::Algorithms::fixAllDurations(*lastItv, newSecDur);
  for(auto& proc : lastItv->processes)
    proc.setParentDuration(ExpandMode::Scale, newSecDur);

  setDuration(actualDuration);
}

Selection SequenceModel::selectableChildren() const noexcept
{
  Selection s;
  for(auto& itv : intervals)
    s.append(&itv);
  for(auto& ev : events)
    s.append(&ev);
  for(auto& ts : timeSyncs)
    s.append(&ts);
  for(auto& st : states)
    s.append(&st);
  return s;
}

Selection SequenceModel::selectedChildren() const noexcept
{
  Selection s;
  for(auto& itv : intervals)
    if(itv.selection.get())
      s.append(&itv);
  for(auto& ev : events)
    if(ev.selection.get())
      s.append(&ev);
  for(auto& ts : timeSyncs)
    if(ts.selection.get())
      s.append(&ts);
  for(auto& st : states)
    if(st.selection.get())
      s.append(&st);
  return s;
}

void SequenceModel::setSelection(const Selection& s) const noexcept
{
  for(auto& itv : intervals)
    itv.selection.set(s.contains(&itv));
  for(auto& ev : events)
    ev.selection.set(s.contains(&ev));
  for(auto& ts : timeSyncs)
    ts.selection.set(s.contains(&ts));
  for(auto& st : states)
    st.selection.set(s.contains(&st));
}

} // namespace Sequence
