#pragma once
#include <Fx/Looper/TakeWriter.hpp>
#include <Fx/Looper/ValueLoop.hpp>

#include <ossia/network/value/value.hpp>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace Nodes::Looper
{
/**
 * A value take: what is heard of a value loop, or of one of its passes, as
 * a small JSON file, which the Automation process reads back.
 *
 *   {
 *     "format": "score-value-take",
 *     "version": 1,
 *     "rate": 48000,            // frames a second
 *     "frames": 96000,          // the loop's length
 *     "tempo": 120,             // what it was played at, and in
 *     "meter": [4, 4],
 *     "components": 1,          // what an automation can play of it: the
 *                               // most numbers a value has, or 0 when the
 *                               // values are not all numbers, vectors and
 *                               // lists of numbers, or have more than
 *                               // max_components
 *     "min": [0.1], "max": [0.9],   // of each component, when there are
 *     "events": [
 *       [0, "float", 0.25],     // [frame, type, value], by frame: a value
 *       [12000, "impulse"],     // held until the next one, an impulse
 *       [24000, "vec3f", [1, 2, 3]],                 // fired on its frame
 *       [36000, "list", [["int", 1], ["string", "a"]]],
 *       [48000, "map", {"x": ["bool", true]}]
 *     ]
 *   }
 *
 * The types are those of ossia: impulse, int, float, bool, string, vec2f,
 * vec3f, vec4f, list, map; a list or a map holds [type, value] pairs. The
 * first value holds from the loop's start when it is on frame 0, else the
 * last one holds there, the loop going round.
 */
struct value_export
{
  static constexpr int max_items = value_loop::max_events;
  struct item
  {
    int64_t pos;
    value_slot value;
    bool impulse;
  };
  std::array<item, max_items> items{};
  int count{};
  int64_t length{};
  double rate{48000.};
  double tempo{120.};
  int meter_numerator{4};
  int meter_denominator{4};
  //! A job is writing it: the processing thread does not fill it until the
  //! job is over. The job may outlive the looper that made it.
  std::atomic_bool in_use{};

  void clear() noexcept
  {
    for(int i = 0; i < count; i++)
      items[i].value.reset();
    count = 0;
  }
  void add(int64_t pos, const value_slot& v, bool impulse)
  {
    if(count >= max_items)
      return;
    auto& it = items[count++];
    it.pos = pos;
    it.value = v;
    it.impulse = impulse;
  }

  //! Most numbers of a value an automation plays, each in a curve of its
  //! own.
  static constexpr int max_components = 32;

  static bool is_number(const ossia::value& v) noexcept
  {
    const auto t = v.get_type();
    return t == ossia::val_type::FLOAT || t == ossia::val_type::INT
           || t == ossia::val_type::BOOL;
  }

  //! The numbers of a value an automation plays: 1 for a number, n for a
  //! vector or a list of n numbers, 0 for a list of none, -1 for anything
  //! else.
  static int numbers_of(const ossia::value& v) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::FLOAT:
      case ossia::val_type::INT:
      case ossia::val_type::BOOL:
        return 1;
      case ossia::val_type::VEC2F:
        return 2;
      case ossia::val_type::VEC3F:
        return 3;
      case ossia::val_type::VEC4F:
        return 4;
      case ossia::val_type::LIST: {
        const auto& l = v.get<std::vector<ossia::value>>();
        for(const auto& e : l)
          if(!is_number(e))
            return -1;
        return int(l.size());
      }
      default:
        return -1;
    }
  }
  //! Number `component` of a value, which it has.
  static double number(const ossia::value& v, int component) noexcept
  {
    switch(v.get_type())
    {
      case ossia::val_type::LIST: {
        const auto& l = v.get<std::vector<ossia::value>>();
        return component >= 0 && component < int(l.size()) ? number(l[component], 0) : 0.;
      }
      case ossia::val_type::FLOAT:
        return v.get<float>();
      case ossia::val_type::INT:
        return v.get<int>();
      case ossia::val_type::BOOL:
        return v.get<bool>() ? 1. : 0.;
      case ossia::val_type::VEC2F:
        return v.get<ossia::vec2f>()[std::clamp(component, 0, 1)];
      case ossia::val_type::VEC3F:
        return v.get<ossia::vec3f>()[std::clamp(component, 0, 2)];
      case ossia::val_type::VEC4F:
        return v.get<ossia::vec4f>()[std::clamp(component, 0, 3)];
      default:
        return 0.;
    }
  }

  //! What an automation can play of the take: see the file's "components".
  //! A value with fewer numbers than others, a list, leaves the ones it does
  //! not have where they were.
  int components() const noexcept
  {
    int n = 0;
    for(int i = 0; i < count; i++)
    {
      if(items[i].impulse)
        return 0;
      const int k = numbers_of(items[i].value.get());
      if(k < 0)
        return 0;
      n = std::max(n, k);
    }
    return n <= max_components ? n : 0;
  }

  std::string to_json() const
  {
    std::string out;
    out.reserve(256 + std::size_t(count) * 32);
    out += "{\n  \"format\": \"score-value-take\",\n  \"version\": 1,\n  \"rate\": ";
    write_number(out, rate);
    out += ",\n  \"frames\": ";
    out += std::to_string(length);
    out += ",\n  \"tempo\": ";
    write_number(out, tempo);
    out += ",\n  \"meter\": [";
    out += std::to_string(meter_numerator);
    out += ", ";
    out += std::to_string(meter_denominator);
    out += "],\n  \"components\": ";
    const int n = components();
    out += std::to_string(n);
    if(n > 0)
    {
      std::array<double, max_components> lo, hi;
      std::array<bool, max_components> seen{};
      lo.fill(0.);
      hi.fill(0.);
      for(int i = 0; i < count; i++)
      {
        const auto& v = items[i].value.get();
        const int k = numbers_of(v);
        for(int c = 0; c < k; c++)
        {
          const double x = number(v, c);
          lo[c] = !seen[c] ? x : std::min(lo[c], x);
          hi[c] = !seen[c] ? x : std::max(hi[c], x);
          seen[c] = true;
        }
      }
      for(const auto& [name, arr] : {std::pair{",\n  \"min\": [", &lo}, {"],\n  \"max\": [", &hi}})
      {
        out += name;
        for(int c = 0; c < n; c++)
        {
          if(c > 0)
            out += ", ";
          write_number(out, (*arr)[c]);
        }
      }
      out += "]";
    }
    out += ",\n  \"events\": [";
    for(int i = 0; i < count; i++)
    {
      out += i == 0 ? "\n    [" : ",\n    [";
      out += std::to_string(items[i].pos);
      if(items[i].impulse)
        out += ", \"impulse\"";
      else
      {
        out += ", ";
        write_typed(out, items[i].value.get(), true);
      }
      out += "]";
    }
    out += count > 0 ? "\n  ]\n}\n" : "]\n}\n";
    return out;
  }

  static void write_number(std::string& out, double x)
  {
    if(!std::isfinite(x))
    {
      out += "0";
      return;
    }
    fmt::format_to(std::back_inserter(out), "{}", x);
  }
  static void write_number(std::string& out, float x)
  {
    if(!std::isfinite(x))
    {
      out += "0";
      return;
    }
    fmt::format_to(std::back_inserter(out), "{}", x);
  }
  static void write_string(std::string& out, std::string_view s)
  {
    out += '"';
    for(char ch : s)
    {
      switch(ch)
      {
        case '"':
          out += "\\\"";
          break;
        case '\\':
          out += "\\\\";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          if(static_cast<unsigned char>(ch) < 0x20)
          {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
            out += buf;
          }
          else
            out += ch;
      }
    }
    out += '"';
  }

  //! "type", value: inside an event, without brackets; inside a list or a
  //! map, as a [type, value] pair.
  static void write_typed(std::string& out, const ossia::value& v, bool flat)
  {
    if(!flat)
      out += '[';
    auto vec = [&](const char* name, const auto& a) {
      out += '"';
      out += name;
      out += "\", [";
      for(std::size_t i = 0; i < a.size(); i++)
      {
        if(i > 0)
          out += ", ";
        write_number(out, a[i]);
      }
      out += ']';
    };
    switch(v.get_type())
    {
      case ossia::val_type::FLOAT:
        out += "\"float\", ";
        write_number(out, v.get<float>());
        break;
      case ossia::val_type::INT:
        out += "\"int\", ";
        out += std::to_string(v.get<int>());
        break;
      case ossia::val_type::BOOL:
        out += v.get<bool>() ? "\"bool\", true" : "\"bool\", false";
        break;
      case ossia::val_type::STRING:
        out += "\"string\", ";
        write_string(out, v.get<std::string>());
        break;
      case ossia::val_type::VEC2F:
        vec("vec2f", v.get<ossia::vec2f>());
        break;
      case ossia::val_type::VEC3F:
        vec("vec3f", v.get<ossia::vec3f>());
        break;
      case ossia::val_type::VEC4F:
        vec("vec4f", v.get<ossia::vec4f>());
        break;
      case ossia::val_type::LIST: {
        out += "\"list\", [";
        bool first = true;
        for(const auto& e : v.get<std::vector<ossia::value>>())
        {
          if(!first)
            out += ", ";
          first = false;
          write_typed(out, e, false);
        }
        out += ']';
        break;
      }
      case ossia::val_type::MAP: {
        out += "\"map\", {";
        bool first = true;
        for(const auto& [k, e] : v.get<ossia::value_map_type>())
        {
          if(!first)
            out += ", ";
          first = false;
          write_string(out, k);
          out += ": ";
          write_typed(out, e, false);
        }
        out += '}';
        break;
      }
      case ossia::val_type::IMPULSE:
      default:
        out += "\"impulse\"";
        break;
    }
    if(!flat)
      out += ']';
  }
};

//! What is heard of `snap` (item -1), or of one of its passes, into `e`: the
//! held values where they change, and the impulses. `tl` and `scratch` are
//! where it is worked out.
inline void fill_value_export(
    const value_loop& snap, int item, held_timeline& tl, held_scratch& scratch,
    value_export& e)
{
  e.clear();
  tl.update(snap, 0, scratch, item, item, item < 0, true);
  // The held values, then the impulses, by position.
  for(int i = 0; i < tl.count; i++)
    if(tl.points[i].event >= 0)
      e.add(tl.points[i].pos, snap.events[tl.points[i].event].value, false);
  for(int i = 0; i < snap.event_count; i++)
  {
    const auto& ev = snap.events[i];
    if(!ev.impulse || !snap.heard(ev, item < 0) || (item >= 0 && ev.pass != item))
      continue;
    const int64_t period = snap.period_for(ev.pass);
    for(int64_t q = ev.pos % period; q < snap.length; q += period)
      e.add(q, ev.value, true);
  }
  std::sort(e.items.begin(), e.items.begin() + e.count, [](const auto& a, const auto& b) {
    return a.pos != b.pos ? a.pos < b.pos : a.impulse < b.impulse;
  });
  e.length = snap.length;
}

//! What the value looper hands over for the take its stop leaves: the loop
//! as it was, and how its files are written. Made on the main thread; the
//! processing thread fills it only while no job has it.
struct value_stop_take
{
  value_loop loop;
  double rate{48000.};
  double tempo{120.};
  int meter_numerator{4};
  int meter_denominator{4};
  stop_take_notice notice;
  std::atomic_bool in_use{};
};

struct value_take_job
{
  std::shared_ptr<take_writer> writer;
  std::shared_ptr<value_export> data;
  //! The take the score's stop leaves: every file of it, written whatever
  //! the looper does meanwhile, from `stop` rather than from `data`.
  std::shared_ptr<value_stop_take> stop;
  int slot{};
  //! Number of the take, 0 for a new one; -1 for the mix, else the pass.
  int number{};
  int layer{-1};
  bool alone{};
  //! What an automation can play of it, worked out by the job, and whether
  //! it holds nothing, which an automation has nothing to play of either.
  //! For the take the score's stop leaves, 1 when an automation can play
  //! each of its files that holds something, else 0.
  int components{};
  bool nothing{};

  bool write(const value_export& e)
  {
    const std::string bytes = e.to_json();
    const auto& path = writer->next_path(slot, number, layer);
    std::ofstream f{
        std::filesystem::path{std::u8string_view{
            reinterpret_cast<const char8_t*>(path.data()), path.size()}},
        std::ios::binary | std::ios::trunc};
    f.write(bytes.data(), std::streamsize(bytes.size()));
    return bool(f);
  }

  bool run()
  {
    if(stop)
      return run_stop();
    if(!data)
      return false;
    struct release
    {
      value_export& e;
      ~release() { e.in_use = false; }
    } done{*data};
    if(!writer || data->length <= 0)
      return false;
    components = data->components();
    nothing = data->count == 0;
    return write(*data);
  }

  //! The mix, then each pass, under one take number. The values of the loop
  //! as it was are let go of here.
  bool run_stop()
  {
    auto& s = *stop;
    struct release
    {
      value_stop_take& s;
      ~release()
      {
        s.loop.clear();
        s.in_use = false;
      }
    } done{s};
    bool ok = writer && s.loop.length > 0;
    bool placeable = true;
    if(ok)
    {
      auto e = std::make_unique<value_export>();
      auto tl = std::make_unique<held_timeline>();
      auto scratch = std::make_unique<held_scratch>();
      e->rate = s.rate;
      e->tempo = s.tempo;
      e->meter_numerator = s.meter_numerator;
      e->meter_denominator = s.meter_denominator;
      number = 0;
      for(layer = -1; ok && layer < s.loop.count; layer++)
      {
        fill_value_export(s.loop, layer, *tl, *scratch, *e);
        // A take goes in the timeline when each of its files that holds
        // something does: a layer whose values are all replaced holds none.
        if(layer < 0 || e->count > 0)
          placeable = placeable && e->components() > 0;
        ok = write(*e);
      }
      e->clear();
    }
    components = placeable ? 1 : 0;
    nothing = false;
    if(writer)
      s.notice.written(*writer, slot, ok, s.loop.count, placeable);
    return ok;
  }
};
}
