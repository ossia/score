#pragma once
#include <Process/Dataflow/WidgetInlets.hpp>

#include <isf.hpp>

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

namespace Gfx
{
/**
 * @brief The control of an ISF / CSF / geometry-filter `long` input.
 *
 * With VALUES it is a ComboBox whose entries carry those values (LABELS name
 * them when there are as many), initially on the entry `def` indexes. Without
 * VALUES it is an IntSpinBox over whichever of MIN and MAX are given, the int
 * range on a side that is not, starting at `def`: a bare `long` used to become
 * an arbitrary 0 / 1 / 2 ComboBox, and one with a single bound too (shader
 * board U11).
 */
inline Process::ControlInlet* makeLongInputControl(
    const isf::long_input& v, const QString& name, Id<Process::Port> id,
    QObject* parent)
{
  if(v.values.empty())
  {
    constexpr int64_t lo = std::numeric_limits<int>::lowest();
    constexpr int64_t hi = std::numeric_limits<int>::max();
    const int min = (int)std::clamp(v.min.value_or(lo), lo, hi);
    const int max = (int)std::clamp(v.max.value_or(hi), (int64_t)min, hi);
    const int def = (int)std::clamp((int64_t)v.def, (int64_t)min, (int64_t)max);
    return new Process::IntSpinBox(min, max, def, name, id, parent);
  }

  std::vector<std::pair<QString, ossia::value>> alternatives;
  const bool labelled = v.labels.size() == v.values.size();
  for(std::size_t idx = 0; idx < v.values.size(); idx++)
  {
    const auto& val = v.values[idx];
    int value = (int)idx;
    QString text;
    if(auto i = ossia::get_if<int64_t>(&val))
    {
      value = int(*i);
      text = QString::number(*i);
    }
    else if(auto d = ossia::get_if<double>(&val))
    {
      value = int(*d);
      text = QString::number(*d);
    }
    else if(auto s = ossia::get_if<std::string>(&val))
    {
      text = QString::fromStdString(*s);
    }
    if(labelled)
      text = QString::fromStdString(v.labels[idx]);
    alternatives.emplace_back(std::move(text), value);
  }

  // ComboBox takes the value initially selected; libisf's `def` is the index
  // into VALUES.
  const ossia::value init
      = alternatives[std::min<std::size_t>(v.def, alternatives.size() - 1)].second;
  return new Process::ComboBox(std::move(alternatives), init, name, id, parent);
}
}
