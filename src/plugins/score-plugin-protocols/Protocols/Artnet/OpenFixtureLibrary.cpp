#include <ossia/detail/config.hpp>
#if defined(OSSIA_PROTOCOL_ARTNET)
#include "OpenFixtureLibrary.hpp"

#include <ossia/detail/algorithms.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/json.hpp>
#include <ossia/detail/parse_relax.hpp>

#include <re2/re2.h>

#include <cmath>
#include <ctre.hpp>

#include <array>
#include <functional>
#include <limits>
#include <memory>
#include <numeric>

namespace Protocols::OFL
{
namespace
{
using JValue = rapidjson::Value;
using Artnet::Channel;
using Artnet::RangeCapability;
using Artnet::SingleCapability;
using ChannelMap = ossia::hash_map<QString, Channel>;

static constexpr auto leq_rex = ctre::match<R"_(<=([0-9]+))_">;
static constexpr auto geq_rex = ctre::match<R"_(>=([0-9]+))_">;
static constexpr auto lst_rex = ctre::match<R"_(<([0-9]+))_">;
static constexpr auto gst_rex = ctre::match<R"_(>([0-9]+))_">;
static constexpr auto eq_rex = ctre::match<R"_(=([0-9]+))_">;
static constexpr auto arith1_rex = ctre::match<R"_(([0-9]+)n)_">;
static constexpr auto arith_rex = ctre::match<R"_(([0-9]+)n\+([0-9]+))_">;

constexpr QLatin1String pixelKeyVariable{"$pixelKey"};
constexpr int maxPixels = 65536;

struct QualifierProperty
{
  std::string_view name, start, end;
};

constexpr std::array qualifierProperties{
    QualifierProperty{"speed", "speedStart", "speedEnd"},
    QualifierProperty{"duration", "durationStart", "durationEnd"},
    QualifierProperty{"time", "timeStart", "timeEnd"},
    QualifierProperty{"brightness", "brightnessStart", "brightnessEnd"},
    QualifierProperty{
        "colorTemperature", "colorTemperatureStart", "colorTemperatureEnd"},
    QualifierProperty{"angle", "angleStart", "angleEnd"},
    QualifierProperty{"horizontalAngle", "horizontalAngleStart", "horizontalAngleEnd"},
    QualifierProperty{"verticalAngle", "verticalAngleStart", "verticalAngleEnd"},
    QualifierProperty{"distance", "distanceStart", "distanceEnd"},
    QualifierProperty{"openPercent", "openPercentStart", "openPercentEnd"},
    QualifierProperty{"frostIntensity", "frostIntensityStart", "frostIntensityEnd"},
    QualifierProperty{"insertion", "insertionStart", "insertionEnd"},
    QualifierProperty{"fogOutput", "fogOutputStart", "fogOutputEnd"},
    QualifierProperty{"parameter", "parameterStart", "parameterEnd"},
    QualifierProperty{"shakeSpeed", "shakeSpeedStart", "shakeSpeedEnd"},
    QualifierProperty{"shakeAngle", "shakeAngleStart", "shakeAngleEnd"},
    QualifierProperty{
        "soundSensitivity", "soundSensitivityStart", "soundSensitivityEnd"},
};

const JValue* member(const JValue& v, std::string_view key) noexcept
{
  if(!v.IsObject())
    return nullptr;
  auto it = v.FindMember(rapidjson::StringRef(key.data(), key.size()));
  return it != v.MemberEnd() ? &it->value : nullptr;
}

std::string_view toStringView(const JValue* v) noexcept
{
  if(v && v->IsString())
    return {v->GetString(), v->GetStringLength()};
  return {};
}

QString toQString(const JValue& v)
{
  if(v.IsString())
    return QString::fromUtf8(v.GetString(), v.GetStringLength());
  return {};
}

QString toQString(const JValue* v)
{
  return v ? toQString(*v) : QString{};
}

bool toBool(const JValue* v) noexcept
{
  return v && v->IsBool() && v->GetBool();
}

QString scalarText(const JValue& v)
{
  if(v.IsString())
    return toQString(v);
  if(v.IsNumber())
    return QString::number(v.GetDouble());
  return {};
}

int64_t maxValue(int bytes) noexcept
{
  return (int64_t{1} << (8 * bytes)) - 1;
}

int clampToInt(int64_t v) noexcept
{
  return int(std::clamp<int64_t>(v, 0, std::numeric_limits<int>::max()));
}

int64_t roundToDmx(double v) noexcept
{
  if(!(v > 0.))
    return 0;
  return std::llround(std::min(v, double(maxValue(4))));
}

QString rangeText(QString a, const QString& b)
{
  if(a != b)
    a += QStringLiteral(" to ") % b;
  return a;
}

std::optional<std::pair<int, int>> dmxRange(const JValue& capa)
{
  auto r = member(capa, "dmxRange");
  if(!r || !r->IsArray() || r->Size() != 2)
    return std::nullopt;
  const auto& a = (*r)[0];
  const auto& b = (*r)[1];
  if(!a.IsNumber() || !b.IsNumber())
    return std::nullopt;
  return std::pair{
      clampToInt(roundToDmx(a.GetDouble())), clampToInt(roundToDmx(b.GetDouble()))};
}

std::optional<int64_t> dmxValue(const JValue& v, int resolution)
{
  if(v.IsNumber())
    return roundToDmx(v.GetDouble());

  auto str = toStringView(&v);
  if(str.empty() || str.back() != '%')
    return std::nullopt;
  str.remove_suffix(1);
  if(auto percent = ossia::parse_relax<double>(str))
    return int64_t(std::floor(std::clamp(*percent, 0., 100.) / 100. * maxValue(resolution)));
  return std::nullopt;
}

Artnet::MenuClick menuClick(std::string_view v) noexcept
{
  if(v == "center")
    return Artnet::MenuClick::Center;
  if(v == "end")
    return Artnet::MenuClick::End;
  if(v == "hidden")
    return Artnet::MenuClick::Hidden;
  return Artnet::MenuClick::Start;
}

bool naturalLess(QStringView a, QStringView b) noexcept
{
  qsizetype i = 0, j = 0;
  while(i < a.size() && j < b.size())
  {
    if(a[i].isDigit() && b[j].isDigit())
    {
      qsizetype ei = i, ej = j;
      while(ei < a.size() && a[ei].isDigit())
        ei++;
      while(ej < b.size() && b[ej].isDigit())
        ej++;
      const auto na = a.sliced(i, ei - i).toULongLong();
      const auto nb = b.sliced(j, ej - j).toULongLong();
      if(na != nb)
        return na < nb;
      i = ei;
      j = ej;
    }
    else
    {
      if(a[i] != b[j])
        return a[i] < b[j];
      i++;
      j++;
    }
  }
  return (a.size() - i) < (b.size() - j);
}

std::function<bool(int)> positionConstraint(std::string_view cst)
{
  if(cst == "even")
    return [](int p) { return (p % 2) == 0; };
  if(cst == "odd")
    return [](int p) { return (p % 2) == 1; };
  if(auto [whole, n] = leq_rex(cst); whole)
    return [num = n.to_number()](int p) { return p <= num; };
  if(auto [whole, n] = geq_rex(cst); whole)
    return [num = n.to_number()](int p) { return p >= num; };
  if(auto [whole, n] = lst_rex(cst); whole)
    return [num = n.to_number()](int p) { return p < num; };
  if(auto [whole, n] = gst_rex(cst); whole)
    return [num = n.to_number()](int p) { return p > num; };
  if(auto [whole, n] = eq_rex(cst); whole)
    return [num = n.to_number()](int p) { return p == num; };
  if(auto [whole, n, r] = arith_rex(cst); whole)
  {
    if(int num = n.to_number(); num > 0)
      return [num, rem = r.to_number()](int p) { return (p % num) == rem; };
  }
  else if(auto [whole, n] = arith1_rex(cst); whole)
  {
    if(int num = n.to_number(); num > 0)
      return [num](int p) { return (p % num) == 0; };
  }
  return [](int) { return false; };
}

Channel instantiateTemplate(const Channel& tmpl, const QString& pixelKey)
{
  Channel ch = tmpl;
  ch.name.replace(pixelKeyVariable, pixelKey);
  for(auto& fine : ch.fineChannels)
    fine.replace(pixelKeyVariable, pixelKey);
  return ch;
}

struct Pixel
{
  int16_t x{}, y{}, z{};
  QString name;
};

struct PixelGroup
{
  QString name;
  std::vector<int> pixels;
};

struct WheelSlot
{
  QString type;
  QString name;
};

struct Wheel
{
  QString name;
  std::vector<WheelSlot> slots;
};

struct SwitchTarget
{
  QString controller;
  std::pair<int, int> range;
  QString target;
};

using SwitchMap = ossia::hash_map<QString, std::vector<SwitchTarget>>;

struct TemplateKey
{
  QString key;
  qsizetype prefix{};
  qsizetype occurrences{};
  qsizetype fixedLength{};
  const std::vector<SwitchTarget>* switches{};
};

class FixtureParser
{
public:
  std::optional<FixtureDefinition> parse(const JValue& doc)
  {
    if(!doc.IsObject() || member(doc, "redirectTo"))
      return std::nullopt;

    FixtureDefinition def;
    def.name = toQString(member(doc, "name"));
    if(def.name.isEmpty())
      return std::nullopt;

    if(auto cats = member(doc, "categories"); cats && cats->IsArray())
      for(const auto& c : cats->GetArray())
        if(c.IsString())
          def.categories.push_back(toQString(c));

    if(auto w = member(doc, "wheels"))
      loadWheels(*w);
    if(auto mat = member(doc, "matrix"))
      loadMatrix(*mat);
    if(auto ch = member(doc, "availableChannels"))
      m_channels = loadChannels(*ch, false);
    if(auto ch = member(doc, "templateChannels"))
      m_templates = loadChannels(*ch, true);
    buildTemplateKeys();
    buildSwitchedFineSlots();

    if(auto modes = member(doc, "modes"); modes && modes->IsArray())
    {
      def.modes.reserve(modes->Size());
      for(const auto& mode : modes->GetArray())
      {
        auto channels = member(mode, "channels");
        if(!channels || !channels->IsArray())
          continue;

        FixtureMode& m = def.modes.emplace_back();
        m.name = toQString(member(mode, "name"));
        m.allChannels.reserve(channels->Size());
        for(const auto& channel : channels->GetArray())
        {
          if(channel.IsString())
            addModeChannel(m, toQString(channel));
          else if(channel.IsObject())
            insertMatrixChannels(m, channel);
          else
            m.allChannels.emplace_back();
        }
      }
    }
    return def;
  }

private:
  void loadWheels(const JValue& wheels)
  {
    if(!wheels.IsObject())
      return;
    for(const auto& w : wheels.GetObject())
    {
      Wheel& wh = m_wheels.emplace_back();
      wh.name = toQString(w.name);
      if(auto s = member(w.value, "slots"); s && s->IsArray())
      {
        for(const auto& slot : s->GetArray())
          wh.slots.push_back(
              {toQString(member(slot, "type")), toQString(member(slot, "name"))});
      }
    }
  }

  QString wheelSlotName(const QString& wheel, int slot) const
  {
    for(const auto& wh : m_wheels)
    {
      if(wh.name != wheel)
        continue;
      if(slot >= 1 && slot <= std::ssize(wh.slots))
      {
        const auto& s = wh.slots[slot - 1];
        return s.name.isEmpty() ? s.type : s.name;
      }
      break;
    }
    return {};
  }

  void loadPixelKeys(const JValue& keys)
  {
    for(int16_t z = 0; z < int16_t(keys.Size()); ++z)
    {
      if(!keys[z].IsArray())
        continue;
      const auto& rows = keys[z];
      for(int16_t y = 0; y < int16_t(rows.Size()); ++y)
      {
        if(!rows[y].IsArray())
          continue;
        const auto& row = rows[y];
        for(int16_t x = 0; x < int16_t(row.Size()); ++x)
          if(row[x].IsString())
            m_pixels.push_back(Pixel{.x = x, .y = y, .z = z, .name = toQString(row[x])});
      }
    }
  }

  void loadPixelCount(const JValue& count)
  {
    if(count.Size() != 3)
      return;
    for(const auto& c : count.GetArray())
      if(!c.IsInt() || c.GetInt() < 1 || c.GetInt() > 1024)
        return;

    const int16_t dim[3]{
        int16_t(count[0].GetInt()), int16_t(count[1].GetInt()),
        int16_t(count[2].GetInt())};
    if(dim[0] * dim[1] * dim[2] > maxPixels)
      return;
    const bool hasX = dim[0] > 1, hasY = dim[1] > 1, hasZ = dim[2] > 1;
    const int axes = hasX + hasY + hasZ;

    m_pixels.reserve(dim[0] * dim[1] * dim[2]);
    for(int16_t z = 0; z < dim[2]; z++)
      for(int16_t y = 0; y < dim[1]; y++)
        for(int16_t x = 0; x < dim[0]; x++)
        {
          QString name;
          if(axes <= 1)
            name = QString::number(std::max({x, y, z}) + 1);
          else if(axes == 2)
            name = QStringLiteral("(%1, %2)")
                       .arg((hasX ? x : y) + 1)
                       .arg((hasY ? y : z) + 1);
          else
            name = QStringLiteral("(%1, %2, %3)").arg(x + 1).arg(y + 1).arg(z + 1);
          m_pixels.push_back(Pixel{.x = x, .y = y, .z = z, .name = std::move(name)});
        }
  }

  std::vector<int> filterPixels(const JValue& constraints) const
  {
    std::vector<std::function<bool(const Pixel&)>> filters;
    auto addPosition = [&](std::string_view axis, int16_t Pixel::* coord) {
      if(auto c = member(constraints, axis); c && c->IsArray())
        for(const auto& v : c->GetArray())
          if(v.IsString())
            filters.push_back([f = positionConstraint(toStringView(&v)),
                               coord](const Pixel& p) { return f(p.*coord + 1); });
    };
    addPosition("x", &Pixel::x);
    addPosition("y", &Pixel::y);
    addPosition("z", &Pixel::z);

    if(auto c = member(constraints, "name"); c && c->IsArray())
    {
      for(const auto& v : c->GetArray())
      {
        auto pattern = toStringView(&v);
        if(pattern.empty())
          continue;
        auto re
            = std::make_shared<RE2>(re2::StringPiece(pattern.data(), pattern.size()));
        if(!re->ok())
          return {};
        filters.push_back([re](const Pixel& p) {
          return RE2::PartialMatch(p.name.toStdString(), *re);
        });
      }
    }

    std::vector<int> matching;
    for(int i = 0; i < std::ssize(m_pixels); i++)
      if(ossia::all_of(filters, [&](const auto& f) { return f(m_pixels[i]); }))
        matching.push_back(i);
    return matching;
  }

  void loadMatrix(const JValue& mat)
  {
    if(auto keys = member(mat, "pixelKeys"); keys && keys->IsArray())
      loadPixelKeys(*keys);
    else if(auto count = member(mat, "pixelCount"); count && count->IsArray())
      loadPixelCount(*count);

    m_pixelIndex.reserve(m_pixels.size());
    for(int i = 0; i < std::ssize(m_pixels); i++)
      m_pixelIndex.emplace(m_pixels[i].name, i);

    auto groups = member(mat, "pixelGroups");
    if(!groups || !groups->IsObject())
      return;

    for(const auto& g : groups->GetObject())
    {
      PixelGroup gp;
      gp.name = toQString(g.name);
      if(toStringView(&g.value) == "all")
      {
        gp.pixels.resize(m_pixels.size());
        std::iota(gp.pixels.begin(), gp.pixels.end(), 0);
      }
      else if(g.value.IsArray())
      {
        for(const auto& px : g.value.GetArray())
          if(auto it = m_pixelIndex.find(toQString(px)); it != m_pixelIndex.end())
            gp.pixels.push_back(it->second);
      }
      else if(g.value.IsObject())
      {
        gp.pixels = filterPixels(g.value);
      }
      m_groupIndex.emplace(gp.name, int(m_groups.size()));
      m_groups.push_back(std::move(gp));
    }
  }

  QString wheelCapabilityName(
      const JValue& capa, const QString& type, const QString& channelName) const
  {
    QString wheel = channelName;
    if(auto w = member(capa, "wheel"))
    {
      if(w->IsString())
        wheel = toQString(*w);
      else if(w->IsArray() && !w->Empty() && (*w)[0].IsString())
        wheel = toQString((*w)[0]);
    }

    auto slotLabel = [&](const JValue& n) -> QString {
      if(n.IsInt())
        return wheelSlotName(wheel, n.GetInt());
      if(n.IsNumber())
      {
        const double v = n.GetDouble();
        return QStringLiteral("Split %1 %2")
            .arg(wheelSlotName(wheel, std::floor(v)))
            .arg(wheelSlotName(wheel, std::ceil(v)));
      }
      return {};
    };

    QString label;
    if(auto n = member(capa, "slotNumber"))
    {
      label = slotLabel(*n);
    }
    else if(auto s = member(capa, "slotNumberStart"))
    {
      if(auto e = member(capa, "slotNumberEnd"))
      {
        label = rangeText(slotLabel(*s), slotLabel(*e));
      }
    }

    if(label.isEmpty())
      return {};
    if(type == QLatin1String("WheelSlot"))
      return label;
    return type % ' ' % label;
  }

  QString capabilityName(
      const JValue& capa, const QString& type, const QString& comment,
      const QString& channelName) const
  {
    if(auto effect = toQString(member(capa, "effectName")); !effect.isEmpty())
      return effect;
    if(type == QLatin1String("NoFunction"))
      return QStringLiteral("No function");
    if(type.startsWith(QLatin1String("Wheel")))
      if(auto name = wheelCapabilityName(capa, type, channelName); !name.isEmpty())
        return name;

    QString name;
    if(type == QLatin1String("ShutterStrobe"))
      name = toQString(member(capa, "shutterEffect"));
    else if(type == QLatin1String("ColorIntensity"))
      name = toQString(member(capa, "color"));
    else if(type == QLatin1String("Effect"))
      name = toQString(member(capa, "effectPreset"));
    else if(type == QLatin1String("FogType"))
      name = toQString(member(capa, "fogType"));
    if(name.isEmpty())
      name = type;

    for(const auto& prop : qualifierProperties)
    {
      QString q;
      if(auto v = member(capa, prop.name))
      {
        q = scalarText(*v);
      }
      else if(auto s = member(capa, prop.start))
      {
        if(auto e = member(capa, prop.end))
        {
          q = rangeText(scalarText(*s), scalarText(*e));
        }
      }
      if(!q.isEmpty())
      {
        name += ' ';
        name += q;
      }
    }
    if(toBool(member(capa, "randomTiming")))
      name += QStringLiteral(" random");
    if(toBool(member(capa, "soundControlled")))
      name += QStringLiteral(" sound");

    if(name == type && !comment.isEmpty())
      return comment;
    return name;
  }

  void collectSwitches(
      const QString& controller, const JValue& capa, std::pair<int, int> range,
      bool isTemplate)
  {
    auto sw = member(capa, "switchChannels");
    if(!sw || !sw->IsObject())
      return;
    auto& switching = isTemplate ? m_templateSwitching : m_switching;
    for(const auto& s : sw->GetObject())
      switching[toQString(s.name)].push_back({controller, range, toQString(s.value)});
  }

  Channel loadChannel(const QString& name, const JValue& jchan, bool isTemplate)
  {
    Channel chan;
    chan.name = name;
    chan.resolution = 1;
    if(!jchan.IsObject())
      return chan;

    if(auto fine = member(jchan, "fineChannelAliases"); fine && fine->IsArray())
    {
      for(const auto& f : fine->GetArray())
      {
        if(!f.IsString())
        {
          chan.fineChannels.clear();
          break;
        }
        chan.fineChannels.push_back(toQString(f));
      }
    }

    int resolution = 1 + int(chan.fineChannels.size());
    if(auto r = toStringView(member(jchan, "dmxValueResolution")); !r.empty())
      if(auto bits = ossia::parse_relax<int>(r); bits && *bits > 0 && *bits % 8 == 0)
        resolution = *bits / 8;
    resolution = std::clamp(resolution, 1, 4);
    chan.resolution = resolution;

    if(auto dv = member(jchan, "defaultValue"))
      if(auto v = dmxValue(*dv, resolution))
        chan.defaultValue = clampToInt(std::min(*v, maxValue(resolution)));

    chan.constant = toBool(member(jchan, "constant"));

    if(auto cap = member(jchan, "capability"); cap && cap->IsObject())
    {
      SingleCapability c;
      c.type = toQString(member(*cap, "type"));
      c.comment = toQString(member(*cap, "comment"));
      c.effectName = toQString(member(*cap, "effectName"));
      chan.capabilities = std::move(c);
      collectSwitches(name, *cap, {0, clampToInt(maxValue(resolution))}, isTemplate);
    }
    else if(auto caps = member(jchan, "capabilities"); caps && caps->IsArray())
    {
      std::vector<RangeCapability> ranges;
      ranges.reserve(caps->Size());
      for(const auto& capa : caps->GetArray())
      {
        auto range = dmxRange(capa);
        QString type = toQString(member(capa, "type"));
        if(!range || type.isEmpty())
          continue;

        RangeCapability& rc = ranges.emplace_back();
        rc.range = *range;
        rc.menuClick = menuClick(toStringView(member(capa, "menuClick")));
        rc.comment = toQString(member(capa, "comment"));
        rc.effectName = capabilityName(capa, type, rc.comment, name);
        rc.type = std::move(type);
        collectSwitches(name, capa, *range, isTemplate);
      }
      chan.capabilities = std::move(ranges);
    }
    return chan;
  }

  ChannelMap loadChannels(const JValue& obj, bool isTemplate)
  {
    ChannelMap channels;
    if(!obj.IsObject())
      return channels;
    channels.reserve(obj.MemberCount());
    for(const auto& c : obj.GetObject())
    {
      QString name = toQString(c.name);
      channels.emplace(name, loadChannel(name, c.value, isTemplate));
    }
    return channels;
  }

  void buildTemplateKeys()
  {
    m_templateKeys.reserve(m_templates.size() + m_templateSwitching.size());
    auto add = [this](const QString& key, const std::vector<SwitchTarget>* switches) {
      const auto idx = key.indexOf(pixelKeyVariable);
      if(idx < 0)
        return;
      const auto occurrences = key.count(pixelKeyVariable);
      m_templateKeys.push_back(
          {key, idx, occurrences, key.size() - occurrences * pixelKeyVariable.size(),
           switches});
    };
    for(const auto& [key, _] : m_templates)
      add(key, nullptr);
    for(const auto& [key, switches] : m_templateSwitching)
      if(!m_templates.contains(key))
        add(key, &switches);
  }

  struct ChannelResolver
  {
    const FixtureParser& self;
    std::optional<Channel> operator()(const QString& name) const
    {
      if(auto it = self.m_channels.find(name); it != self.m_channels.end())
        return it->second;
      return self.templateReference(name);
    }
    QString fineSlot(const QString& fine) const { return self.fineSlot(fine); }
  };

  struct TemplateResolver
  {
    const FixtureParser& self;
    QString pixelKey;
    std::optional<Channel> operator()(const QString& name) const
    {
      if(auto it = self.m_templates.find(name); it != self.m_templates.end())
        return instantiateTemplate(it->second, pixelKey);
      return std::nullopt;
    }
    QString fineSlot(const QString& fine) const { return fine; }
  };

  QString fineSlot(const QString& fine) const
  {
    auto it = m_switchedFine.find(fine);
    return it != m_switchedFine.end() ? it->second : fine;
  }

  void buildSwitchedFineSlots()
  {
    ossia::hash_set<QString> fines;
    for(const auto& [_, chan] : m_channels)
      for(const auto& f : chan.fineChannels)
        fines.insert(f);
    if(fines.empty())
      return;

    for(const auto& [alias, switches] : m_switching)
      if(auto active = activeTarget(switches, ChannelResolver{*this});
         fines.contains(active))
        m_switchedFine.try_emplace(std::move(active), alias);
  }

  bool isSwitchedFineSlot(const QString& alias) const
  {
    return ossia::any_of(
        m_switchedFine, [&](const auto& kv) { return kv.second == alias; });
  }

  template <typename Resolver>
  static QString
  activeTarget(const std::vector<SwitchTarget>& switches, const Resolver& resolve)
  {
    if(switches.empty())
      return {};
    const QString& controller = switches.front().controller;
    if(auto ctl = resolve(controller))
    {
      const int dv = ctl->defaultValue;
      for(const auto& s : switches)
        if(s.controller == controller && s.range.first <= dv && dv <= s.range.second)
          return s.target;
    }
    return switches.front().target;
  }

  std::optional<Channel> templateReference(const QString& name) const
  {
    for(const auto& tk : m_templateKeys)
    {
      const auto variable = name.size() - tk.fixedLength;
      if(variable <= 0 || variable % tk.occurrences != 0)
        continue;
      if(!QStringView{name}.startsWith(QStringView{tk.key}.first(tk.prefix)))
        continue;
      const QString pixelKey = name.sliced(tk.prefix, variable / tk.occurrences);
      if(!m_pixelIndex.contains(pixelKey) && !m_groupIndex.contains(pixelKey))
        continue;
      if(QString{tk.key}.replace(pixelKeyVariable, pixelKey) != name)
        continue;
      if(tk.switches)
        return switchedChannel(name, *tk.switches, TemplateResolver{*this, pixelKey});
      return instantiateTemplate(m_templates.find(tk.key)->second, pixelKey);
    }
    return std::nullopt;
  }

  template <typename Resolver>
  static std::optional<Channel> switchedChannel(
      const QString& alias, const std::vector<SwitchTarget>& switches,
      const Resolver& resolve)
  {
    std::vector<QString> keys;
    std::vector<Channel> targets;
    for(const auto& s : switches)
    {
      if(s.target.isEmpty() || ossia::contains(keys, s.target))
        continue;
      if(auto t = resolve(s.target))
      {
        keys.push_back(s.target);
        targets.push_back(std::move(*t));
      }
    }
    if(targets.empty())
      return std::nullopt;

    const int active
        = std::max(0, ossia::index_in_container(keys, activeTarget(switches, resolve)));
    const Channel& current = targets[active];

    Channel ch;
    ch.name = alias;
    ch.resolution = std::max<int8_t>(current.resolution, 1);
    ch.defaultValue = current.defaultValue;
    ch.constant = current.constant;
    for(const auto& fine : current.fineChannels)
      ch.fineChannels.push_back(resolve.fineSlot(fine));

    std::vector<RangeCapability> caps;
    const bool prefixed = targets.size() > 1;
    for(const Channel& t : targets)
    {
      const int from = std::max<int8_t>(t.resolution, 1);
      if(auto* ranges = ossia::get_if<std::vector<RangeCapability>>(&t.capabilities))
      {
        for(const auto& r : *ranges)
        {
          RangeCapability& c = caps.emplace_back(r);
          const auto [start, end]
              = scaleRange(r.range.first, r.range.second, from, ch.resolution);
          c.range = {clampToInt(start), clampToInt(end)};
          const QString& n = r.effectName.isEmpty() ? r.type : r.effectName;
          c.effectName = prefixed ? QString{t.name % QStringLiteral(": ") % n} : n;
        }
      }
      else if(auto* single = ossia::get_if<SingleCapability>(&t.capabilities))
      {
        RangeCapability& c = caps.emplace_back();
        c.type = single->type;
        c.comment = single->comment;
        c.effectName = t.name;
        c.range = {0, clampToInt(maxValue(ch.resolution))};
      }
    }
    ch.capabilities = std::move(caps);
    return ch;
  }

  void addModeChannel(FixtureMode& m, QString name)
  {
    if(auto it = m_channels.find(name); it != m_channels.end())
    {
      Channel& ch = m.channels.emplace_back(it->second);
      for(auto& fine : ch.fineChannels)
        fine = fineSlot(fine);
    }
    else if(auto sw = m_switching.find(name); sw != m_switching.end())
    {
      if(!isSwitchedFineSlot(name))
        if(auto ch = switchedChannel(name, sw->second, ChannelResolver{*this}))
          m.channels.push_back(std::move(*ch));
    }
    else if(auto ch = templateReference(name))
      m.channels.push_back(std::move(*ch));
    m.allChannels.push_back(std::move(name));
  }

  std::vector<QString> repeatedPixelKeys(const JValue& repeatFor) const
  {
    std::vector<QString> keys;
    if(repeatFor.IsArray())
    {
      for(const auto& k : repeatFor.GetArray())
        if(k.IsString())
          keys.push_back(toQString(k));
      return keys;
    }

    std::string_view rf = toStringView(&repeatFor);
    if(rf == "eachPixelGroup")
    {
      for(const auto& g : m_groups)
        keys.push_back(g.name);
    }
    else if(rf == "eachPixelABC")
    {
      for(const auto& p : m_pixels)
        keys.push_back(p.name);
      std::sort(keys.begin(), keys.end(), naturalLess);
    }
    else if(rf.size() == 12 && rf.starts_with("eachPixel"))
    {
      std::array<int16_t Pixel::*, 3> axes;
      for(int i = 0; i < 3; i++)
      {
        switch(rf[9 + i])
        {
          case 'X':
            axes[i] = &Pixel::x;
            break;
          case 'Y':
            axes[i] = &Pixel::y;
            break;
          case 'Z':
            axes[i] = &Pixel::z;
            break;
          default:
            return keys;
        }
      }

      auto sorted = m_pixels;
      auto order = [&](const Pixel& p) {
        return std::array{p.*(axes[2]), p.*(axes[1]), p.*(axes[0])};
      };
      std::stable_sort(
          sorted.begin(), sorted.end(),
          [&](const Pixel& a, const Pixel& b) { return order(a) < order(b); });
      for(auto& p : sorted)
        keys.push_back(std::move(p.name));
    }
    return keys;
  }

  void insertMatrixChannels(FixtureMode& m, const JValue& insert)
  {
    if(toStringView(member(insert, "insert")) != "matrixChannels")
    {
      m.allChannels.emplace_back();
      return;
    }
    auto repeatFor = member(insert, "repeatFor");
    if(!repeatFor)
      return;

    std::vector<QString> templateNames;
    if(auto t = member(insert, "templateChannels"); t && t->IsArray())
      for(const auto& c : t->GetArray())
        templateNames.push_back(toQString(c));

    auto add = [&](const QString& tmpl, const QString& pixelKey) {
      if(tmpl.isEmpty())
      {
        m.allChannels.emplace_back();
        return;
      }
      QString slot = QString{tmpl}.replace(pixelKeyVariable, pixelKey);
      if(auto it = m_channels.find(slot); it != m_channels.end())
        m.channels.push_back(it->second);
      else if(auto it = m_templates.find(tmpl); it != m_templates.end())
        m.channels.push_back(instantiateTemplate(it->second, pixelKey));
      else if(auto sw = m_templateSwitching.find(tmpl); sw != m_templateSwitching.end())
        if(auto ch
           = switchedChannel(slot, sw->second, TemplateResolver{*this, pixelKey}))
          m.channels.push_back(std::move(*ch));
      m.allChannels.push_back(std::move(slot));
    };

    const auto keys = repeatedPixelKeys(*repeatFor);
    if(toStringView(member(insert, "channelOrder")) == "perChannel")
    {
      for(const auto& tmpl : templateNames)
        for(const auto& key : keys)
          add(tmpl, key);
    }
    else
    {
      for(const auto& key : keys)
        for(const auto& tmpl : templateNames)
          add(tmpl, key);
    }
  }

  std::vector<Wheel> m_wheels;
  std::vector<Pixel> m_pixels;
  std::vector<PixelGroup> m_groups;
  ossia::hash_map<QString, int> m_pixelIndex;
  ossia::hash_map<QString, int> m_groupIndex;
  ChannelMap m_channels;
  ChannelMap m_templates;
  std::vector<TemplateKey> m_templateKeys;
  SwitchMap m_switching;
  SwitchMap m_templateSwitching;
  ossia::hash_map<QString, QString> m_switchedFine;
};
}

QString FixtureMode::content() const noexcept
{
  QString str;
  str.reserve(500);
  int k = 0;
  for(const auto& chan : allChannels)
  {
    str += QString::number(++k);
    str += QStringLiteral(": \t");
    str += chan.isEmpty() ? QStringLiteral("<No function>") : chan;
    str += '\n';
  }
  return str;
}

std::optional<FixtureDefinition> parseFixture(std::string_view json)
{
  rapidjson::Document doc;
  doc.Parse(json.data(), json.size());
  if(doc.HasParseError())
    return std::nullopt;
  return FixtureParser{}.parse(doc);
}

std::optional<FixtureDefinition> parseFixture(
    std::string_view json,
    const std::function<std::optional<std::string>(const QString&)>& readFixture)
{
  rapidjson::Document doc;
  doc.Parse(json.data(), json.size());
  if(doc.HasParseError() || !doc.IsObject())
    return std::nullopt;

  const QString target = toQString(member(doc, "redirectTo"));
  if(target.isEmpty())
    return FixtureParser{}.parse(doc);

  if(toStringView(member(doc, "reason")) != "SameAsDifferentBrand" || !readFixture)
    return std::nullopt;
  const auto targetJson = readFixture(target);
  if(!targetJson)
    return std::nullopt;
  auto def = parseFixture(*targetJson);
  if(def)
    if(QString name = toQString(member(doc, "name")); !name.isEmpty())
      def->name = std::move(name);
  return def;
}

int64_t scaleValue(int64_t value, int from, int to) noexcept
{
  from = std::clamp(from, 1, 4);
  to = std::clamp(to, 1, 4);
  value = std::clamp<int64_t>(value, 0, maxValue(from));
  if(to <= from)
    return value >> (8 * (from - to));
  const int64_t last = value & 0xFF;
  for(int k = from; k < to; k++)
    value = (value << 8) | last;
  return value;
}

std::pair<int64_t, int64_t>
scaleRange(int64_t start, int64_t end, int from, int to) noexcept
{
  from = std::clamp(from, 1, 4);
  to = std::clamp(to, 1, 4);
  start = std::clamp<int64_t>(start, 0, maxValue(from));
  end = std::clamp<int64_t>(end, 0, maxValue(from));
  if(to >= from)
  {
    const int pad = 8 * (to - from);
    return {start << pad, (end << pad) | maxValue(to - from)};
  }

  end >>= 8 * (from - to);
  for(int r = from; r > to; r--)
  {
    const int64_t removed = start & 0xFF;
    start >>= 8;
    if(removed > 0 && start < end)
      start++;
  }
  return {start, end};
}

std::optional<int64_t>
menuClickValue(const Artnet::RangeCapability& capability, int from, int to) noexcept
{
  const auto [start, end]
      = scaleRange(capability.range.first, capability.range.second, from, to);
  switch(capability.menuClick)
  {
    case Artnet::MenuClick::Center:
      return (start + end) / 2;
    case Artnet::MenuClick::End:
      return end;
    case Artnet::MenuClick::Hidden:
      return std::nullopt;
    default:
      return start;
  }
}
}
#endif
