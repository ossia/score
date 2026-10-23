#pragma once

// Loads the presets of an object from a package's Presets folder (the .scp
// files score saves) and applies them to the object's `inputs`, the way the
// avnd process maps them: preset entry N is input field N, ports included.

#include <avnd/common/aggregates.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/introspection/widgets.hpp>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <cstdlib>
#include <string>
#include <type_traits>
#include <vector>

namespace midi_presets
{
struct preset
{
  QString file;
  QString name;
  QString category;
  QString uuid;
  QJsonArray values;
};

//! SCORE_PRESETS_ROOT, or the default package of the user library.
inline QString presets_root()
{
  if(const char* env = std::getenv("SCORE_PRESETS_ROOT"))
    return QString::fromUtf8(env);
  return QDir::homePath() + "/Documents/ossia/score/packages/default/Presets";
}

//! Every .scp of `folder` (relative to presets_root()) whose key is `uuid`.
inline std::vector<preset> load(const QString& folder, const QString& uuid)
{
  std::vector<preset> res;
  QDir dir{presets_root() + "/" + folder};
  for(const auto& fi : dir.entryInfoList({"*.scp"}, QDir::Files, QDir::Name))
  {
    QFile f{fi.absoluteFilePath()};
    if(!f.open(QIODevice::ReadOnly))
      continue;
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    const auto key = obj["Key"].toObject();
    if(key["Uuid"].toString().compare(uuid, Qt::CaseInsensitive) != 0)
      continue;
    res.push_back(
        {fi.absoluteFilePath(), obj["Name"].toString(), obj["Category"].toString(),
         key["Uuid"].toString(), obj["Preset"].toArray()});
  }
  return res;
}

namespace detail
{
template <typename Field>
void assign(
    Field& field, const QJsonObject& v, QStringList& errors, int idx, double tempo)
{
  auto fail = [&](const char* what) {
    errors << QStringLiteral("entry %1: %2").arg(idx).arg(what);
  };

  if constexpr(!requires { field.value; })
  {
    fail("not a control");
  }
  else
  {
    using V = std::decay_t<decltype(field.value)>;
    const auto num = [&]() -> double {
      if(v.contains("Float"))
        return v["Float"].toDouble();
      if(v.contains("Int"))
        return v["Int"].toInt();
      if(v.contains("Bool"))
        return v["Bool"].toBool() ? 1. : 0.;
      fail("expected a number");
      return 0.;
    };

    if constexpr(requires { field.sync; })
    {
      // Time chooser: [time, synced], or a plain number of seconds
      if(v.contains("Vec2f"))
      {
        const auto a = v["Vec2f"].toArray();
        if(a.size() != 2)
          return fail("expected two numbers for a time");
        // The ossia binding hands the object seconds even when synced: a note
        // value (1 = whole note) converted at the current tempo.
        field.sync = a[1].toDouble() > 0.5;
        field.value = field.sync ? V(4. * a[0].toDouble() * 60. / tempo)
                                 : V(a[0].toDouble());
      }
      else
      {
        field.value = V(num());
        field.sync = false;
      }
    }
    else if constexpr(requires { field.value.start; field.value.end; })
    {
      const auto a = v["Vec2f"].toArray();
      if(a.size() != 2)
        return fail("expected Vec2f for a range");
      field.value.start = a[0].toDouble();
      field.value.end = a[1].toDouble();
    }
    else if constexpr(std::is_same_v<V, bool>)
    {
      field.value = num() > 0.5;
    }
    else if constexpr(std::is_same_v<V, std::string>)
    {
      if(!v.contains("String"))
        return fail("expected String");
      field.value = v["String"].toString().toStdString();
    }
    else if constexpr(std::is_enum_v<V>)
    {
      if(v.contains("String"))
      {
        static constexpr auto range = avnd::get_range<Field>();
        const auto names = avnd::to_enum_range(range.values);
        const auto s = v["String"].toString().toStdString();
        for(std::size_t i = 0; i < names.size(); i++)
          if(names[i] == s)
            return void(field.value = static_cast<V>(i));
        fail("unknown enum value");
      }
      else
      {
        field.value = static_cast<V>(int(num()));
      }
    }
    else if constexpr(std::is_arithmetic_v<V>)
    {
      field.value = V(num());
    }
    else
    {
      fail("unsupported control type");
    }
  }
}
}

//! Applies a preset to `inputs`. Returns the problems found, empty when every
//! entry matched a control of the right type. Synced times are converted to
//! seconds at `tempo`, as the ossia binding does.
template <typename Inputs>
QStringList apply(Inputs& inputs, const preset& p, double tempo = 120.)
{
  QStringList errors;
  for(const auto& entry : p.values)
  {
    const auto pair = entry.toArray();
    if(pair.size() != 2)
    {
      errors << "malformed entry";
      continue;
    }
    const int idx = pair[0].toInt();
    const auto val = pair[1].toObject();
    bool found = false;
    int n = 0;
    avnd::pfr::for_each_field(inputs, [&](auto& field) {
      if(n++ == idx)
      {
        found = true;
        detail::assign(field, val, errors, idx, tempo);
      }
    });
    if(!found)
      errors << QStringLiteral("entry %1: no such input").arg(idx);
  }
  return errors;
}
}
