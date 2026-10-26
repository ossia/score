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
//! The numbers of a [type, value] of the take, or none.
std::optional<std::array<double, 4>> numbers(const QJsonArray& ev, int components)
{
  std::array<double, 4> r{};
  const QString type = ev.at(1).toString();
  const QJsonValue v = ev.at(2);
  if(type == QStringLiteral("float") || type == QStringLiteral("int"))
  {
    if(components != 1 || !v.isDouble())
      return {};
    r[0] = v.toDouble();
    return r;
  }
  if(type == QStringLiteral("bool"))
  {
    if(components != 1 || !v.isBool())
      return {};
    r[0] = v.toBool() ? 1. : 0.;
    return r;
  }
  if(type.startsWith(QStringLiteral("vec")))
  {
    const auto a = v.toArray();
    if(a.size() != components)
      return {};
    for(int i = 0; i < components; i++)
      r[i] = a.at(i).toDouble();
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
  if(t.components < 1 || t.components > 4 || frames <= 0. || rate <= 0.)
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
  for(const auto& e : obj.value(QStringLiteral("events")).toArray())
  {
    const auto ev = e.toArray();
    auto n = numbers(ev, t.components);
    if(!n)
      return {};
    t.steps.push_back({std::clamp(ev.at(0).toDouble() / frames, 0., 1.), *n});
  }
  if(t.steps.empty())
    return {};
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
    p.creation.prettyName
        = take->components == 1
              ? data.filename.basename
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
