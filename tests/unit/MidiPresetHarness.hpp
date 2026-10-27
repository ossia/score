#pragma once

// Loads the presets of an object from a package's Presets folder (the .scp
// files score saves) and applies them to the object's `inputs`, the way the
// avnd process maps them: preset entry N is input field N, ports included,
// and an entry that names its control names that field.
//
// The presets come from tests/fixtures/presets, a copy of the folders of the
// default package that the tests read; SCORE_PRESETS_ROOT points them at
// another library, e.g. to check a package before it is published.

#include <avnd/common/aggregates.hpp>
#include <avnd/introspection/range.hpp>
#include <avnd/introspection/widgets.hpp>

#include <ossia/network/value/value.hpp>

#include <halp/grid_chooser.hpp>
#include <halp/string_list.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QDir>
#include <QFileInfo>
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

//! SCORE_PRESETS_ROOT, or the fixtures of the source tree the test was built from.
inline QString presets_root()
{
  if(const char* env = std::getenv("SCORE_PRESETS_ROOT"); env && *env)
    return QString::fromUtf8(env);
#if defined(SCORE_TEST_PRESETS_ROOT)
  return QStringLiteral(SCORE_TEST_PRESETS_ROOT);
#else
  return {};
#endif
}

//! Every .scp of `folder` (relative to presets_root()) whose key is `uuid`.
//! Skips the running test when the folder is absent: the binary was moved away
//! from its source tree, and counting what is there would test nothing.
inline std::vector<preset> load(const QString& folder, const QString& uuid)
{
  std::vector<preset> res;
  const QString root = presets_root();
  QDir dir{root + "/" + folder};
  if(root.isEmpty() || !dir.exists())
    SKIP(
        "no preset folder " << QString(root + "/" + folder).toStdString()
                            << " (set SCORE_PRESETS_ROOT)");
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

    if constexpr(std::is_same_v<V, halp::grid_value>)
    {
      // Grid chooser: [value, unit]. A plain number is what the control was
      // before, converted as the avnd process converts it on load.
      if(v.contains("Vec2f"))
      {
        const auto a = v["Vec2f"].toArray();
        if(a.size() != 2)
          return fail("expected two numbers for a grid");
        field.value = {halp::grid_unit(int(a[1].toDouble())), a[0].toDouble(), 0.};
      }
      else if constexpr(requires(const ossia::value& o) { Field::upgrade_value(o); })
      {
        const ossia::value up = Field::upgrade_value(ossia::value{float(num())});
        if(auto g = up.template target<ossia::vec2f>())
          field.value = {halp::grid_unit(int((*g)[1])), double((*g)[0]), 0.};
        else
          fail("upgrade_value did not give a grid");
      }
      else
      {
        field.value = halp::grid_value::seconds(num());
      }
    }
    else if constexpr(requires { field.sync; })
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
    else if constexpr(std::is_same_v<V, halp::string_list_value>)
    {
      // List of strings: {"Tuple":[{"Tuple":[{"Int":id},{"String":text}]}]}.
      // A text saved before the control was a list goes through the field's
      // upgrade_value, as the avnd process does on load.
      ossia::value list;
      if(v.contains("Tuple"))
      {
        std::vector<ossia::value> rows;
        for(const auto& elt : v["Tuple"].toArray())
        {
          const auto pair = elt.toObject()["Tuple"].toArray();
          if(pair.size() != 2 || !pair[0].toObject().contains("Int")
             || !pair[1].toObject().contains("String"))
            return fail("expected [Int, String] rows");
          rows.push_back(std::vector<ossia::value>{
              pair[0].toObject()["Int"].toInt(),
              pair[1].toObject()["String"].toString().toStdString()});
        }
        list = std::move(rows);
      }
      else if(v.contains("String"))
      {
        if constexpr(requires(const ossia::value& o) { Field::upgrade_value(o); })
          list = Field::upgrade_value(
              ossia::value{v["String"].toString().toStdString()});
        else
          return fail("expected Tuple");
      }
      else
      {
        return fail("expected Tuple");
      }
      field.value.clear();
      if(auto rows = list.template target<std::vector<ossia::value>>())
        for(const auto& row : *rows)
          if(auto pair = row.template target<std::vector<ossia::value>>();
             pair && pair->size() == 2)
            if(auto id = (*pair)[0].template target<int>())
              if(auto text = (*pair)[1].template target<std::string>())
                field.value.emplace_back(*id, *text);
    }
    else if constexpr(requires { field.value.size(); typename V::value_type; }
                      && !std::is_same_v<V, std::string>)
    {
      // List control: {"Tuple":[elements]}, an element being a number or, for
      // aggregates, a Tuple of numbers. Arrays take exactly their size.
      using E = typename V::value_type;
      if(!v.contains("Tuple"))
        return fail("expected Tuple");
      constexpr bool resizable = requires { field.value.clear(); };
      const auto elts = v["Tuple"].toArray();
      if constexpr(resizable)
        field.value.clear();
      else if(std::size_t(elts.size()) != field.value.size())
        return fail("wrong number of elements");
      std::size_t idx = 0;
      for(const auto& elt : elts)
      {
        const auto o = elt.toObject();
        auto number = [&](const QJsonObject& x) -> double {
          if(x.contains("Float"))
            return x["Float"].toDouble();
          if(x.contains("Int"))
            return x["Int"].toInt();
          if(x.contains("Bool"))
            return x["Bool"].toBool() ? 1. : 0.;
          fail("expected a number in the list");
          return 0.;
        };
        E e{};
        if constexpr(std::is_arithmetic_v<E>)
        {
          e = E(number(o));
        }
        else
        {
          const auto sub = o["Tuple"].toArray();
          int k = 0;
          avnd::pfr::for_each_field(e, [&](auto& m) {
            if(k < sub.size())
              m = std::decay_t<decltype(m)>(number(sub[k].toObject()));
            k++;
          });
        }
        if constexpr(resizable)
        {
          if(field.value.size() < field.value.capacity())
            field.value.push_back(e);
          else
            fail("too many elements");
        }
        else
        {
          field.value[idx++] = e;
        }
      }
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
    if(pair.size() != 2 && pair.size() != 3)
    {
      errors << "malformed entry";
      continue;
    }
    const int idx = pair[0].toInt();
    const auto val = pair[1].toObject();
    // [index, value, name]: the name is the input's at that index.
    const QString name = pair.size() == 3 ? pair[2].toString() : QString{};
    bool found = false;
    int n = 0;
    avnd::pfr::for_each_field(inputs, [&](auto& field) {
      if(n++ == idx)
      {
        found = true;
        using F = std::decay_t<decltype(field)>;
        if constexpr(requires { F::name(); })
        {
          const std::string_view nm = F::name();
          if(!name.isEmpty() && name != QString::fromUtf8(nm.data(), qsizetype(nm.size())))
            errors << QStringLiteral("entry %1: named %2, the input is %3")
                          .arg(idx)
                          .arg(name)
                          .arg(QString::fromUtf8(nm.data(), qsizetype(nm.size())));
        }
        detail::assign(field, val, errors, idx, tempo);
      }
    });
    if(!found)
      errors << QStringLiteral("entry %1: no such input").arg(idx);
  }
  return errors;
}
}
