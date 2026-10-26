#include "ValueTakeDrop.hpp"

#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/SetAutomationMax.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace Automation
{
namespace
{
//! A number of the take: a [type, value] of a number type.
std::optional<double> number(const QString& type, const QJsonValue& v)
{
  if((type == QStringLiteral("float") || type == QStringLiteral("int")) && v.isDouble())
    return v.toDouble();
  if(type == QStringLiteral("bool") && v.isBool())
    return v.toBool() ? 1. : 0.;
  return {};
}

//! The numbers of an event of the take, by index, or none when it is not
//! numbers.
std::optional<std::vector<double>> numbers(const QJsonArray& ev, bool& list)
{
  const QString type = ev.at(1).toString();
  const QJsonValue v = ev.at(2);
  if(auto n = number(type, v))
    return std::vector<double>{*n};
  std::vector<double> r;
  if(type.startsWith(QStringLiteral("vec")))
  {
    for(const auto& e : v.toArray())
      r.push_back(e.toDouble());
    return r;
  }
  if(type == QStringLiteral("list"))
  {
    list = true;
    for(const auto& e : v.toArray())
    {
      const auto pair = e.toArray();
      auto n = number(pair.at(0).toString(), pair.at(1));
      if(!n)
        return {};
      r.push_back(*n);
    }
    return r;
  }
  return {};
}
}

std::optional<ValueTake> ValueTake::parse(const QByteArray& json)
{
  const auto doc = QJsonDocument::fromJson(json);
  const auto obj = doc.object();
  if(obj.value(QStringLiteral("format")).toString() != QStringLiteral("score-value-take"))
    return {};
  ValueTake t;
  t.components = obj.value(QStringLiteral("components")).toInt();
  const double frames = obj.value(QStringLiteral("frames")).toDouble();
  const double rate = obj.value(QStringLiteral("rate")).toDouble();
  if(t.components < 1 || t.components > max_components || frames <= 0. || rate <= 0.)
    return {};
  t.seconds = frames / rate;
  const auto lo = obj.value(QStringLiteral("min")).toArray();
  const auto hi = obj.value(QStringLiteral("max")).toArray();
  for(int c = 0; c < t.components; c++)
  {
    t.min[c] = lo.at(c).toDouble();
    t.max[c] = hi.at(c).toDouble();
    // A take that does not move still needs room to be drawn in.
    if(!(t.max[c] > t.min[c]))
      t.max[c] = t.min[c] + 1.;
  }
  // A number a value does not have holds where it was, and before it first
  // comes, its first value holds.
  std::array<double, max_components> held{};
  std::array<int, max_components> first_at;
  first_at.fill(-1);
  for(const auto& e : obj.value(QStringLiteral("events")).toArray())
  {
    const auto ev = e.toArray();
    auto n = numbers(ev, t.lists);
    if(!n || int(n->size()) > t.components)
      return {};
    for(std::size_t c = 0; c < n->size(); c++)
    {
      held[c] = (*n)[c];
      if(first_at[c] < 0)
        first_at[c] = int(t.steps.size());
    }
    t.steps.push_back({std::clamp(ev.at(0).toDouble() / frames, 0., 1.), held});
  }
  if(t.steps.empty())
    return {};
  for(int c = 0; c < t.components; c++)
    for(int i = 0; i < first_at[c]; i++)
      t.steps[i].value[c] = t.steps[first_at[c]].value[c];
  return t;
}

std::vector<Curve::SegmentData> ValueTake::segments(int component) const
{
  std::vector<Curve::SegmentData> segs;
  if(steps.empty() || component < 0 || component >= components)
    return segs;
  const double lo = min[component], hi = max[component];
  auto y = [&](const step& s) {
    return std::clamp((s.value[component] - lo) / (hi - lo), 0., 1.);
  };
  auto add = [&](double x0, double y0, double x1, double y1) {
    Curve::SegmentData d;
    d.id = Id<Curve::SegmentModel>{int(segs.size())};
    d.start = {x0, y0};
    d.end = {x1, y1};
    if(!segs.empty())
    {
      d.previous = segs.back().id;
      segs.back().following = d.id;
    }
    d.type = Metadata<ConcreteKey_k, Curve::LinearSegment>::get();
    d.specificSegmentData = QVariant::fromValue(Curve::LinearSegmentData{});
    segs.push_back(std::move(d));
  };

  // Up to the first value, the last one holds: the loop goes round.
  double x = 0., held = y(steps.back());
  for(const auto& s : steps)
  {
    const double v = y(s);
    if(s.x > x)
    {
      add(x, held, s.x, held);
      x = s.x;
    }
    if(v != held)
    {
      if(segs.empty())
        held = v;
      else
        add(x, held, x, v);
      held = v;
    }
  }
  if(x < 1.)
    add(x, held, 1., held);
  return segs;
}

QSet<QString> ValueTakeDropHandler::fileExtensions() const noexcept
{
  return {QStringLiteral("values")};
}

void ValueTakeDropHandler::dropData(
    std::vector<ProcessDrop>& drops, const DroppedFile& data,
    const score::DocumentContext& ctx) const noexcept
{
  const auto take = ValueTake::parse(data.data);
  if(!take)
    return;
  static constexpr const char* names[4]{"x", "y", "z", "w"};
  for(int c = 0; c < take->components; c++)
  {
    ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, Automation::ProcessModel>::get();
    // A list's numbers by their index, a vector's by their axis.
    p.creation.prettyName
        = take->components == 1 ? data.filename.basename
          : take->lists || take->components > 4
              ? data.filename.basename + QStringLiteral(" ") + QString::number(c)
              : data.filename.basename + QStringLiteral(" ") + QString::fromUtf8(names[c]);
    p.duration = TimeVal::fromMsecs(take->seconds * 1000.);
    p.setup = [segs = take->segments(c), lo = take->min[c], hi = take->max[c]](
                  Process::ProcessModel& m, score::Dispatcher& disp) {
      auto& autom = static_cast<Automation::ProcessModel&>(m);
      disp.submit(new Automation::SetMin{autom, lo});
      disp.submit(new Automation::SetMax{autom, hi});
      disp.submit(new Curve::UpdateCurve{autom.curve(), segs});
    };
    drops.push_back(std::move(p));
  }
}
}
