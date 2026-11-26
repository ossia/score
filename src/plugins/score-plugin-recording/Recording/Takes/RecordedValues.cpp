#include "RecordedValues.hpp"
#include <Process/TakeFiles.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace Recording
{
namespace
{
constexpr int max_components = Process::valueTakeMaxComponents;

QJsonArray typed(const ossia::value& v);

QJsonValue payload(const ossia::value& v)
{
  struct vis
  {
    QJsonValue operator()(ossia::impulse) const { return {}; }
    QJsonValue operator()(int i) const { return i; }
    QJsonValue operator()(float f) const { return double(f); }
    QJsonValue operator()(bool b) const { return b; }
    QJsonValue operator()(const std::string& s) const { return QString::fromStdString(s); }
    QJsonValue operator()(const ossia::vec2f& v) const { return QJsonArray{v[0], v[1]}; }
    QJsonValue operator()(const ossia::vec3f& v) const
    {
      return QJsonArray{v[0], v[1], v[2]};
    }
    QJsonValue operator()(const ossia::vec4f& v) const
    {
      return QJsonArray{v[0], v[1], v[2], v[3]};
    }
    QJsonValue operator()(const std::vector<ossia::value>& l) const
    {
      QJsonArray a;
      for(const auto& e : l)
        a.push_back(typed(e));
      return a;
    }
    QJsonValue operator()(const ossia::value_map_type& m) const
    {
      QJsonObject o;
      for(const auto& [k, e] : m)
        o.insert(QString::fromStdString(k), typed(e));
      return o;
    }
    QJsonValue operator()() const { return {}; }
  };
  return v.apply(vis{});
}

const char* typeName(const ossia::value& v)
{
  switch(v.get_type())
  {
    case ossia::val_type::IMPULSE:
      return "impulse";
    case ossia::val_type::INT:
      return "int";
    case ossia::val_type::FLOAT:
      return "float";
    case ossia::val_type::BOOL:
      return "bool";
    case ossia::val_type::STRING:
      return "string";
    case ossia::val_type::VEC2F:
      return "vec2f";
    case ossia::val_type::VEC3F:
      return "vec3f";
    case ossia::val_type::VEC4F:
      return "vec4f";
    case ossia::val_type::LIST:
      return "list";
    case ossia::val_type::MAP:
      return "map";
    default:
      return "impulse";
  }
}

QJsonArray typed(const ossia::value& v)
{
  QJsonArray a{QString::fromLatin1(typeName(v))};
  if(v.get_type() != ossia::val_type::IMPULSE)
    a.push_back(payload(v));
  return a;
}

}

std::optional<std::vector<double>> numbersOf(const ossia::value& v)
{
  switch(v.get_type())
  {
    case ossia::val_type::INT:
    case ossia::val_type::FLOAT:
    case ossia::val_type::BOOL:
      return std::vector<double>{ossia::convert<double>(v)};
    case ossia::val_type::VEC2F: {
      auto x = v.get<ossia::vec2f>();
      return std::vector<double>{x[0], x[1]};
    }
    case ossia::val_type::VEC3F: {
      auto x = v.get<ossia::vec3f>();
      return std::vector<double>{x[0], x[1], x[2]};
    }
    case ossia::val_type::VEC4F: {
      auto x = v.get<ossia::vec4f>();
      return std::vector<double>{x[0], x[1], x[2], x[3]};
    }
    case ossia::val_type::LIST: {
      std::vector<double> r;
      for(const auto& e : v.get<std::vector<ossia::value>>())
      {
        const auto t = e.get_type();
        if(t != ossia::val_type::INT && t != ossia::val_type::FLOAT
           && t != ossia::val_type::BOOL)
          return std::nullopt;
        r.push_back(ossia::convert<double>(e));
      }
      return r;
    }
    default:
      return std::nullopt;
  }
}

int numericComponents(const std::vector<ValueTake::Event>& events)
{
  int components = 0;
  for(const auto& e : events)
  {
    const auto n = numbersOf(e.value);
    if(!n)
      return 0;
    components = std::max(components, int(n->size()));
  }
  return components <= max_components ? components : 0;
}

QByteArray valueTakeJson(
    const std::vector<ValueTake::Event>& events, int64_t start, int64_t end, int rate,
    double tempo, int meterNumerator, int meterDenominator)
{
  constexpr double flicks_per_second = ossia::flicks_per_second<double>;
  const auto frame_of = [&](int64_t date) {
    return std::max<int64_t>(
        0, std::llround(double(date - start) / flicks_per_second * rate));
  };

  const int components = numericComponents(events);
  std::vector<double> lo(components, std::numeric_limits<double>::max());
  std::vector<double> hi(components, std::numeric_limits<double>::lowest());

  QJsonArray evs;
  for(const auto& e : events)
  {
    QJsonArray ev{double(frame_of(e.date))};
    for(const auto& x : typed(e.value))
      ev.push_back(x);
    evs.push_back(ev);
    if(components > 0)
      if(const auto n = numbersOf(e.value))
        for(std::size_t c = 0; c < n->size(); c++)
        {
          lo[c] = std::min(lo[c], (*n)[c]);
          hi[c] = std::max(hi[c], (*n)[c]);
        }
  }

  QJsonObject o;
  o.insert("format", "score-value-take");
  o.insert("version", 1);
  o.insert("rate", rate);
  o.insert("frames", double(std::max<int64_t>(1, frame_of(end))));
  // The frames count score's time, where a quarter lasts half a second: the
  // tempo that reads them so is the root tempo, whatever the box played at.
  o.insert("tempo", 120.);
  if(tempo > 0.)
    o.insert("recordedTempo", tempo);
  o.insert("meter", QJsonArray{meterNumerator, meterDenominator});
  o.insert("components", components);
  if(components > 0)
  {
    QJsonArray jlo, jhi;
    for(int c = 0; c < components; c++)
    {
      jlo.push_back(lo[c]);
      jhi.push_back(hi[c]);
    }
    o.insert("min", jlo);
    o.insert("max", jhi);
  }
  o.insert("events", evs);
  return QJsonDocument{o}.toJson(QJsonDocument::Compact);
}
}
