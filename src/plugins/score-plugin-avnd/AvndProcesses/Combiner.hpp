#pragma once
#include <ossia/network/base/osc_address.hpp>

#include <AvndProcesses/AddressTools.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <ranges>
#include <span>

namespace avnd_tools
{

struct PatternCombiner : PatternObject
{
  halp_meta(name, "Pattern combiner")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data Processing")
  halp_meta(description, "Apply an operation to all inputs matching a pattern")
  halp_meta(c_name, "avnd_pattern_combine")
  halp_meta(uuid, "18efe965-9acc-4703-9af3-3cef658b301a")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/pattern-combiner.html#pattern-combiner")

  struct
  {
    PatternSelector pattern;

    struct
    {
      halp__enum(
          "Mode", List, List, Average, Sum, Min, Max, Map, ArgMax, ArgMin, Median,
          Count, Any, All, Range, StdDev, Changed, Vote, Sort, TopN);
      halp_meta(
          description,
          "List, Map: every value, as a list or keyed by address. "
          "Average, Sum, Min, Max, Median, Range (max - min), StdDev: lists, "
          "vectors and maps are combined element by element; what is not a "
          "number is left out. "
          "Count, Any, All: how many values pass the test. "
          "ArgMax, ArgMin: the address of the largest or smallest value. "
          "Changed: the values that changed since the last tick, by address. "
          "Vote: the most frequent value. "
          "Sort, TopN: the values by address, smallest first, or the N largest "
          "first. Vectors and lists are ranked by their length.")
    } mode{};

    struct : halp::spinbox_i32<"N", halp::irange{1, 1024, 3}>
    {
      halp_meta(description, "How many values TopN keeps.")
    } n;

    struct
    {
      halp__enum("Test", Above, Above, Below, Inside, Outside);
      halp_meta(
          description,
          "What Count, Any and All count. Above: on above Max, off again "
          "below Min. Below: on below Min, off again above Max. Inside: "
          "between Min and Max. Outside: not between them. What is not a "
          "number never passes.")
    } test{};

    halp::spinbox_f32<"Min", halp::range{-1e6, 1e6, 0.5}> min;
    halp::spinbox_f32<"Max", halp::range{-1e6, 1e6, 0.5}> max;
  } inputs;

  struct
  {
    halp::val_port<"Output", std::optional<ossia::value>> output;
  } outputs;

  std::vector<ossia::value> current_values;
  std::vector<ossia::net::node_base*> current_nodes;

  static constexpr double nan = std::numeric_limits<double>::quiet_NaN();

  //! A value as a number: NaN for what is not one.
  static double numeric(const ossia::value& v) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::INT:
        return *v.target<int>();
      case ossia::val_type::FLOAT:
        return *v.target<float>();
      case ossia::val_type::BOOL:
        return *v.target<bool>() ? 1. : 0.;
      default:
        return nan;
    }
  }

  //! The type all the values share; NONE when they differ.
  static ossia::val_type common_type(const std::vector<ossia::value>& val) noexcept
  {
    const auto t0 = val.empty() ? ossia::val_type::NONE : val[0].get_type();
    for(const auto& e : val)
      if(e.get_type() != t0)
        return ossia::val_type::NONE;
    return t0;
  }

  std::string address(std::size_t i) const
  {
    return ossia::net::address_string_from_node(*current_nodes[i]);
  }

  //! A value as one number, to rank it: numbers as they are, vectors, lists
  //! and maps by their length, over the elements that are numbers. None for
  //! the others, and for NaN, which has no rank.
  static std::optional<double> magnitude(const ossia::value& v) noexcept
  {
    auto length = [](auto&& elements) {
      double s = 0.;
      bool any = false;
      for(double x : elements)
        if(!std::isnan(x))
        {
          s += x * x;
          any = true;
        }
      return any ? std::sqrt(s) : nan;
    };

    double m = nan;
    switch(v.get_type())
    {
      case ossia::val_type::INT:
      case ossia::val_type::FLOAT:
      case ossia::val_type::BOOL:
        m = numeric(v);
        break;
      case ossia::val_type::VEC2F:
        m = length(*v.target<ossia::vec2f>());
        break;
      case ossia::val_type::VEC3F:
        m = length(*v.target<ossia::vec3f>());
        break;
      case ossia::val_type::VEC4F:
        m = length(*v.target<ossia::vec4f>());
        break;
      case ossia::val_type::LIST:
        m = length(*v.target<std::vector<ossia::value>>() | std::views::transform(numeric));
        break;
      case ossia::val_type::MAP:
        m = length(
            *v.target<ossia::value_map_type>()
            | std::views::transform([](const auto& e) { return numeric(e.second); }));
        break;
      default:
        break;
    }
    if(std::isnan(m))
      return std::nullopt;
    return m;
  }

  // ArgMax, ArgMin: the first one by address of the values ranked highest.
  void process_arg(bool largest)
  {
    std::optional<std::size_t> best;
    double best_m{};
    for(std::size_t i = 0; i < current_values.size(); i++)
    {
      const auto m = magnitude(current_values[i]);
      if(!m)
        continue;
      if(!best || (largest ? *m > best_m : *m < best_m))
      {
        best = i;
        best_m = *m;
      }
    }
    if(best)
      outputs.output.value = address(*best);
  }

  // Sort: every value that has a magnitude, smallest first. TopN: the n
  // largest, largest first. Equal ones stay in address order.
  std::vector<std::pair<double, std::size_t>> m_ranked;
  void process_sorted(bool largest_first, std::size_t n)
  {
    m_ranked.clear();
    for(std::size_t i = 0; i < current_values.size(); i++)
      if(auto m = magnitude(current_values[i]))
        m_ranked.emplace_back(*m, i);
    // The address order breaks the ties: neither sort is stable
    auto order = [largest_first](const auto& a, const auto& b) {
      if(a.first != b.first)
        return largest_first ? a.first > b.first : a.first < b.first;
      return a.second < b.second;
    };
    n = std::min(n, m_ranked.size());
    if(n < m_ranked.size())
      std::partial_sort(m_ranked.begin(), m_ranked.begin() + n, m_ranked.end(), order);
    else
      std::sort(m_ranked.begin(), m_ranked.end(), order);

    ossia::value_map_type res;
    res.reserve(n);
    for(std::size_t k = 0; k < n; k++)
    {
      const auto i = m_ranked[k].second;
      res.emplace_back(address(i), current_values[i]);
    }
    outputs.output.value = std::move(res);
  }

  //! Same type and same value; NaN is the same as NaN, so that a sensor stuck
  //! on it neither changes every tick nor splits a vote.
  static bool same(const ossia::value& a, const ossia::value& b) noexcept
  {
    if(a.get_type() != b.get_type())
      return false;
    if(auto fa = a.target<float>())
      return *fa == *b.target<float>() || (std::isnan(*fa) && std::isnan(*b.target<float>()));
    return a == b;
  }

  //! Equal values (as `same` says) hash the same.
  struct value_hash
  {
    static std::size_t mix(std::size_t h, std::size_t v) noexcept
    {
      return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
    }
    static std::size_t of(float f) noexcept
    {
      return std::isnan(f) ? 1 : std::hash<float>{}(f);
    }
    std::size_t operator()(ossia::impulse) const noexcept { return 0; }
    std::size_t operator()(int v) const noexcept { return std::hash<int>{}(v); }
    std::size_t operator()(float v) const noexcept { return of(v); }
    std::size_t operator()(bool v) const noexcept { return v; }
    std::size_t operator()(const std::string& v) const noexcept
    {
      return std::hash<std::string>{}(v);
    }
    template <std::size_t N>
    std::size_t operator()(const std::array<float, N>& v) const noexcept
    {
      std::size_t h = N;
      for(float x : v)
        h = mix(h, of(x));
      return h;
    }
    // Containers by their size: equal ones are told apart by comparing them
    std::size_t operator()(const std::vector<ossia::value>& v) const noexcept
    {
      return v.size();
    }
    std::size_t operator()(const ossia::value_map_type& v) const noexcept
    {
      return v.size();
    }
    std::size_t operator()() const noexcept { return 0; }
  };

  // Vote: the value the most inputs have, the first one by address on a tie.
  // Impulses carry no value to vote for. The values are grouped by hash, then
  // told apart within a group: n log n however many choices there are.
  struct vote
  {
    std::size_t hash;
    std::size_t index;
  };
  std::vector<vote> m_votes;
  std::vector<std::pair<std::size_t, int>> m_vote_group;
  void process_vote()
  {
    m_votes.clear();
    for(std::size_t i = 0; i < current_values.size(); i++)
    {
      const auto& v = current_values[i];
      if(!v.valid() || v.get_type() == ossia::val_type::IMPULSE)
        continue;
      m_votes.push_back({value_hash::mix(std::size_t(v.get_type()), v.apply(value_hash{})), i});
    }
    std::sort(m_votes.begin(), m_votes.end(), [](const vote& a, const vote& b) {
      return a.hash != b.hash ? a.hash < b.hash : a.index < b.index;
    });

    std::optional<std::pair<std::size_t, int>> best;
    for(std::size_t g = 0; g < m_votes.size();)
    {
      // The values of one hash, in address order: usually all equal
      m_vote_group.clear();
      std::size_t e = g;
      for(; e < m_votes.size() && m_votes[e].hash == m_votes[g].hash; e++)
      {
        const auto& v = current_values[m_votes[e].index];
        auto it = ossia::find_if(
            m_vote_group, [&](const auto& c) { return same(current_values[c.first], v); });
        if(it == m_vote_group.end())
          m_vote_group.emplace_back(m_votes[e].index, 1);
        else
          it->second++;
      }
      for(const auto& c : m_vote_group)
        if(!best || c.second > best->second
           || (c.second == best->second && c.first < best->first))
          best = c;
      g = e;
    }
    if(best)
      outputs.output.value = current_values[best->first];
  }

  // Changed: what changed since the last tick Changed ran, by address. On the
  // first one, everything. Nothing goes out when nothing changed.
  std::vector<ossia::net::node_base*> m_previous_nodes;
  std::vector<ossia::value> m_previous_values;
  std::vector<std::pair<ossia::net::node_base*, std::size_t>> m_previous_index;
  bool m_changed_ran{};
  void process_changed(bool continuing)
  {
    const bool same_nodes = continuing && m_previous_nodes == current_nodes;
    if(continuing && !same_nodes)
    {
      // The inputs matched changed: find their previous values by node
      m_previous_index.clear();
      for(std::size_t i = 0; i < m_previous_nodes.size(); i++)
        m_previous_index.emplace_back(m_previous_nodes[i], i);
      std::sort(m_previous_index.begin(), m_previous_index.end());
    }

    ossia::value_map_type res;
    for(std::size_t i = 0; i < current_values.size(); i++)
    {
      const ossia::value* before{};
      if(same_nodes)
        before = &m_previous_values[i];
      else if(continuing)
      {
        auto it = std::lower_bound(
            m_previous_index.begin(), m_previous_index.end(),
            std::pair{current_nodes[i], std::size_t{0}});
        if(it != m_previous_index.end() && it->first == current_nodes[i])
          before = &m_previous_values[it->second];
      }

      if(!before || !same(*before, current_values[i]))
        res.emplace_back(address(i), current_values[i]);
    }
    // Swapped rather than copied: the next tick gathers its values anew
    m_previous_nodes.swap(current_nodes);
    m_previous_values.swap(current_values);
    if(!res.empty())
      outputs.output.value = std::move(res);
  }

  // The reducing modes work on columns of numbers: one per element when all
  // the values are vectors, lists or maps, else one of all the values. A
  // list's index or a map's key that some values lack is a column of the
  // values that have it. What is not a number is NaN there.
  enum class column_shape
  {
    scalar,
    vec,
    list,
    map
  };
  // A pool: the first m_column_count are this tick's, the others keep their
  // memory for the next ones.
  std::vector<std::vector<double>> m_columns;
  std::vector<std::string> m_keys;
  std::size_t m_column_count{};

  std::size_t new_column()
  {
    if(m_column_count == m_columns.size())
      m_columns.emplace_back();
    m_columns[m_column_count].clear();
    return m_column_count++;
  }

  //! The column of a map's key. Maps mostly list their keys in one order: the
  //! one at the key's place is tried first.
  std::size_t key_column(const std::string& k, std::size_t place)
  {
    if(place < m_column_count && m_keys[place] == k)
      return place;
    for(std::size_t c = 0; c < m_column_count; c++)
      if(m_keys[c] == k)
        return c;
    const std::size_t c = new_column();
    if(m_keys.size() <= c)
      m_keys.resize(c + 1);
    m_keys[c] = k;
    return c;
  }

  template <std::size_t N>
  void gather_vec()
  {
    for(std::size_t c = 0; c < N; c++)
      new_column();
    for(const auto& v : current_values)
    {
      const auto& a = *v.template target<std::array<float, N>>();
      for(std::size_t c = 0; c < N; c++)
        m_columns[c].push_back(a[c]);
    }
  }

  column_shape gather_columns()
  {
    m_column_count = 0;
    switch(common_type(current_values))
    {
      case ossia::val_type::VEC2F:
        gather_vec<2>();
        return column_shape::vec;
      case ossia::val_type::VEC3F:
        gather_vec<3>();
        return column_shape::vec;
      case ossia::val_type::VEC4F:
        gather_vec<4>();
        return column_shape::vec;
      case ossia::val_type::LIST:
        for(const auto& v : current_values)
        {
          const auto& l = *v.target<std::vector<ossia::value>>();
          for(std::size_t c = 0; c < l.size(); c++)
          {
            if(c == m_column_count)
              new_column();
            m_columns[c].push_back(numeric(l[c]));
          }
        }
        return column_shape::list;
      case ossia::val_type::MAP:
        for(const auto& v : current_values)
        {
          std::size_t place = 0;
          for(const auto& [k, e] : *v.target<ossia::value_map_type>())
            m_columns[key_column(k, place++)].push_back(numeric(e));
        }
        return column_shape::map;
      default:
        new_column();
        for(const auto& v : current_values)
          m_columns[0].push_back(numeric(v));
        return column_shape::scalar;
    }
  }

  //! f(column, offset) reduces a column, to nothing (an invalid value) when
  //! it has nothing to reduce; offset is where the column starts among all
  //! the gathered numbers. Nothing goes out for a scalar or a map's key that
  //! reduced to nothing, NaN for a vector's or a list's element.
  template <typename F>
  void emit_columns(column_shape s, F f)
  {
    std::size_t offset = 0;
    auto reduce = [&](std::size_t c) {
      auto r = f(m_columns[c], offset);
      offset += m_columns[c].size();
      return r.valid() ? r : ossia::value{float(nan)};
    };

    switch(s)
    {
      case column_shape::scalar: {
        auto r = f(m_columns[0], offset);
        if(r.valid())
          outputs.output.value = std::move(r);
        break;
      }
      case column_shape::vec: {
        // Stays a vector when each element reduces to a number
        std::array<ossia::value, 4> r;
        const std::size_t n = m_column_count;
        bool floats = true;
        for(std::size_t c = 0; c < n; c++)
        {
          r[c] = reduce(c);
          floats &= r[c].get_type() == ossia::val_type::FLOAT;
        }
        auto at = [&r](std::size_t c) { return *r[c].target<float>(); };
        if(!floats)
          outputs.output.value = std::vector<ossia::value>(r.begin(), r.begin() + n);
        else if(n == 2)
          outputs.output.value = ossia::vec2f{at(0), at(1)};
        else if(n == 3)
          outputs.output.value = ossia::vec3f{at(0), at(1), at(2)};
        else
          outputs.output.value = ossia::vec4f{at(0), at(1), at(2), at(3)};
        break;
      }
      case column_shape::list: {
        std::vector<ossia::value> res;
        res.reserve(m_column_count);
        for(std::size_t c = 0; c < m_column_count; c++)
          res.push_back(reduce(c));
        outputs.output.value = std::move(res);
        break;
      }
      case column_shape::map: {
        ossia::value_map_type res;
        res.reserve(m_column_count);
        for(std::size_t c = 0; c < m_column_count; c++)
        {
          auto r = f(m_columns[c], offset);
          offset += m_columns[c].size();
          if(r.valid())
            res.emplace_back(m_keys[c], std::move(r));
        }
        outputs.output.value = std::move(res);
        break;
      }
    }
  }

  // Statistics, of at least one number, none of them NaN.
  using statistic = double (*)(std::span<double>) noexcept;
  static double total(std::span<double> c) noexcept
  {
    double s = 0.;
    for(double x : c)
      s += x;
    return s;
  }
  static double mean(std::span<double> c) noexcept { return total(c) / c.size(); }
  static double smallest(std::span<double> c) noexcept { return *std::ranges::min_element(c); }
  static double largest(std::span<double> c) noexcept { return *std::ranges::max_element(c); }
  static double median(std::span<double> c) noexcept
  {
    const auto mid = c.begin() + c.size() / 2;
    std::nth_element(c.begin(), mid, c.end());
    if(c.size() % 2)
      return *mid;
    return (*mid + *std::max_element(c.begin(), mid)) / 2.;
  }
  static double range(std::span<double> c) noexcept
  {
    const auto [lo, hi] = std::ranges::minmax_element(c);
    return *hi - *lo;
  }
  //! Of the values themselves, not of a sample of them: divided by n.
  static double stddev(std::span<double> c) noexcept
  {
    const double m = mean(c);
    double var = 0.;
    for(double x : c)
      var += (x - m) * (x - m);
    return std::sqrt(var / c.size());
  }

  template <statistic F>
  void process_statistic()
  {
    emit_columns(gather_columns(), [](std::vector<double>& c, std::size_t) {
      // What is not a number is left out; the orderings need NaN gone too
      std::erase_if(c, [](double x) { return std::isnan(x); });
      return c.empty() ? ossia::value{} : ossia::value{float(F(c))};
    });
  }

  // Count, Any, All: whether each number passed the test on the last tick,
  // for the hysteresis of Above and Below. Starts over when what is counted
  // changes.
  std::vector<uint8_t> m_active;
  std::vector<ossia::net::node_base*> m_active_nodes;
  void prepare_active()
  {
    std::size_t total = 0;
    for(std::size_t c = 0; c < m_column_count; c++)
      total += m_columns[c].size();
    if(m_active.size() != total || m_active_nodes != current_nodes)
    {
      m_active.assign(total, 0);
      m_active_nodes = current_nodes;
    }
  }

  bool passes(double v, bool was) const noexcept
  {
    const double lo = std::min(inputs.min.value, inputs.max.value);
    const double hi = std::max(inputs.min.value, inputs.max.value);
    // NaN fails each comparison: it passes no test
    using test = decltype(inputs.test)::enum_type;
    switch(inputs.test)
    {
      case test::Above:
        return was ? v >= lo : v > hi;
      case test::Below:
        return was ? v <= hi : v < lo;
      case test::Inside:
        return v >= lo && v <= hi;
      case test::Outside:
        return v < lo || v > hi;
    }
    return false;
  }

  int count_passing(const std::vector<double>& c, std::size_t offset) noexcept
  {
    int k = 0;
    for(std::size_t i = 0; i < c.size(); i++)
    {
      const bool on = passes(c[i], m_active[offset + i]);
      m_active[offset + i] = on;
      k += on;
    }
    return k;
  }

  template <typename F>
  void process_counting(F f)
  {
    const auto s = gather_columns();
    prepare_active();
    emit_columns(s, [&](const std::vector<double>& c, std::size_t offset) {
      return f(count_passing(c, offset), int(c.size()));
    });
  }

  void operator()()
  {
    using mode = decltype(inputs.mode)::enum_type;
    // Changed compares with the tick before only when it ran then too
    const bool continuing = std::exchange(m_changed_ran, inputs.mode == mode::Changed);
    if(m_paths.empty())
      return;

    current_values.clear();
    current_nodes.clear();
    if(inputs.pattern.observer.dirty)
      inputs.pattern.reprocess();
    for(auto in : this->roots)
    {
      if(auto p = in->get_parameter())
      {
        current_values.push_back(p->value());
        current_nodes.push_back(in);
      }
    }

    if(current_values.empty())
      return;

    switch(inputs.mode)
    {
      case mode::List:
        // Moved: the next tick gathers its values anew
        outputs.output.value = std::move(current_values);
        break;
      case mode::Map: {
        ossia::value_map_type res;
        res.reserve(current_values.size());
        for(std::size_t i = 0; i < current_values.size(); i++)
          res.emplace_back(address(i), current_values[i]);
        outputs.output.value = std::move(res);
        break;
      }
      case mode::Average:
        process_statistic<&mean>();
        break;
      case mode::Sum:
        process_statistic<&total>();
        break;
      case mode::Min:
        process_statistic<&smallest>();
        break;
      case mode::Max:
        process_statistic<&largest>();
        break;
      case mode::Median:
        process_statistic<&median>();
        break;
      case mode::Range:
        process_statistic<&range>();
        break;
      case mode::StdDev:
        process_statistic<&stddev>();
        break;
      case mode::ArgMax:
        process_arg(true);
        break;
      case mode::ArgMin:
        process_arg(false);
        break;
      case mode::Count:
        process_counting([](int k, int) { return ossia::value{k}; });
        break;
      case mode::Any:
        process_counting([](int k, int) { return ossia::value{k > 0}; });
        break;
      case mode::All:
        process_counting([](int k, int n) { return ossia::value{k == n}; });
        break;
      case mode::Changed:
        process_changed(continuing);
        break;
      case mode::Vote:
        process_vote();
        break;
      case mode::Sort:
        process_sorted(false, current_values.size());
        break;
      case mode::TopN:
        process_sorted(true, std::size_t(std::max(1, inputs.n.value)));
        break;
    }
  }
};

}
