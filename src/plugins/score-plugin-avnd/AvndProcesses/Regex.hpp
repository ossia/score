#pragma once
#include <ossia/dataflow/exec_state_facade.hpp>
#include <ossia/dataflow/token_request.hpp>
#include <ossia/dataflow/value_port.hpp>
#include <ossia/detail/thread.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/string_list.hpp>
#include <re2/re2.h>

#include <AvndProcesses/ValueSerialization.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ao
{
/**
 * @brief Regular expressions on the values that arrive, with RE2.
 *
 * One outlet per capture group of the pattern, named after the group when it
 * has a name: `(?P<temp>\d+)` makes a "temp" outlet. The outlets keep their
 * cables when the pattern is edited, as long as the group keeps its name, or
 * its number for unnamed groups.
 *
 * RE2 matches in linear time, whatever the pattern: a pattern cannot stall the
 * audio thread. It has no back-references inside the pattern, nor look-around.
 * Compiling is not bounded that way: an edit of the pattern during playback is
 * compiled by the worker, off the audio thread, within RE2's memory budget.
 */
struct Regex
{
  halp_meta(name, "Regex")
  halp_meta(c_name, "avnd_regex")
  halp_meta(category, "Control/Mappings")
  halp_meta(author, "ossia score")
  halp_meta(
      description,
      "Match, search, replace or split text with a regular expression (RE2 syntax). "
      "Each capture group gets an outlet, named after the group for (?P<name>...). "
      "Numbers and lists are matched as their text.")
  halp_meta(uuid, "ef63f9b2-cc68-4b25-825e-e3b126875086")

  enum Mode
  {
    //! The first match anywhere in the text
    Search,
    //! The whole text must match
    Match,
    //! Every match, in order
    SearchAll,
    //! The text with the matches replaced
    Replace,
    //! The pieces of text between the matches
    Split
  };

  //! Keys of the capture outlets: a pure function of the pattern, so that
  //! reloading a document or editing the pattern finds the same outlets.
  static constexpr int max_capture_outlets = 64;

  struct
  {
    struct
    {
      halp_meta(name, "Input")
      halp_meta(description, "The text to process. Numbers and lists are their text; "
                             "an impulse processes the last input again.")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } input;

    struct : halp::lineedit<"Pattern", "">
    {
      halp_meta(description, "RE2 syntax; (?P<name>...) names a group and its outlet")
      void update(Regex& self) { self.compile(); }
      static std::function<void(Regex&, const std::string&)> on_controller_interaction()
      {
        return [](Regex& self, const std::string& pattern) {
          self.outputs.captures.set_rows(Regex::captureRows(pattern));
        };
      }
    } pattern;

    struct : halp::combobox_t<"Mode", Mode>
    {
      void update(Regex& self) { self.compile(); }
    } mode;

    struct : halp::lineedit<"Replacement", "">
    {
      halp_meta(description, "Replace mode: \\0 is the match, \\1 to \\9 the groups "
                             "($1 and %1 work too)")
      void update(Regex& self) { self.compile(); }
    } replacement;

    struct : halp::toggle<"Global", halp::default_on_toggle>
    {
      halp_meta(description, "Replace mode: every match, or only the first one")
    } global;

    struct : halp::toggle<"Ignore case">
    {
      void update(Regex& self) { self.compile(); }
    } ignore_case;

    struct : halp::toggle<"Multiline">
    {
      halp_meta(description, "^ and $ match at every line")
      void update(Regex& self) { self.compile(); }
    } multiline;

    struct : halp::toggle<"Numbers">
    {
      halp_meta(description, "Groups that are numbers are sent as numbers")
    } numbers;
  } inputs;

  struct capture_port
  {
    halp_meta(name, "Group {}")
    ossia::value_port* value{};
    static constexpr bool is_event() { return true; }
  };

  struct
  {
    struct
    {
      halp_meta(name, "Match")
      halp_meta(description, "The match; Search all: the list of matches; Replace: "
                             "the text replaced; Split: the list of pieces")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } match;
    struct
    {
      halp_meta(name, "Groups")
      halp_meta(description, "The groups of the match, as a list; Search all: a list "
                             "per match")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } groups;
    struct
    {
      halp_meta(name, "Matched")
      halp_meta(description, "true or false, for every input")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } matched;
    struct
    {
      halp_meta(name, "Unmatched")
      halp_meta(description, "The input, when it does not match")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } unmatched;
    struct
    {
      halp_meta(name, "Error")
      halp_meta(description, "Why the pattern or the replacement is not valid; an "
                             "empty string once they are")
      ossia::value_port* value{};
      static constexpr bool is_event() { return true; }
    } error;
    halp::keyed_dynamic_port<capture_port> captures;
  } outputs;

  using tick = ossia::token_request;
  ossia::exec_state_facade ossia_state;

  //! The capture groups of a pattern, in order: their names ("" for unnamed
  //! ones). RE2's own list when the pattern compiles; otherwise read from the
  //! text, so that a typo (an unclosed parenthesis...) keeps the groups and
  //! their cables. Either way a function of the text alone: undo and reload
  //! find the same outlets.
  static std::vector<std::string> captureGroups(const std::string& pattern)
  {
    std::vector<std::string> groups;
    RE2::Options opts;
    opts.set_log_errors(false);
    RE2 re{pattern, opts};
    if(re.ok())
    {
      const auto& names = re.CapturingGroupNames();
      for(int i = 1; i <= re.NumberOfCapturingGroups(); i++)
      {
        auto it = names.find(i);
        groups.push_back(it != names.end() ? it->second : std::string{});
      }
      return groups;
    }

    bool in_class = false;
    for(std::size_t i = 0; i < pattern.size(); i++)
    {
      const char c = pattern[i];
      if(c == '\\')
      {
        i++;
        continue;
      }
      if(in_class)
      {
        in_class = c != ']';
        continue;
      }
      if(c == '[')
      {
        in_class = true;
        continue;
      }
      if(c != '(')
        continue;
      if(i + 1 < pattern.size() && pattern[i + 1] == '?')
      {
        // (?P<name>...) and (?<name>...) capture; (?:...), (?i)... do not.
        std::size_t n = i + 2;
        if(n < pattern.size() && pattern[n] == 'P')
          n++;
        if(n < pattern.size() && pattern[n] == '<')
        {
          const auto close = pattern.find('>', n + 1);
          groups.push_back(
              pattern.substr(n + 1, close == std::string::npos ? std::string::npos
                                                               : close - n - 1));
        }
        continue;
      }
      groups.emplace_back();
    }
    return groups;
  }

  //! One row per capture group: its key and its name ("" for unnamed groups,
  //! which then take the port's "Group {}" name).
  static halp::string_list_value captureRows(const std::string& pattern)
  {
    const auto groups = captureGroups(pattern);
    const int count = std::min<int>(groups.size(), max_capture_outlets);
    halp::string_list_value rows;
    rows.reserve(count);
    std::vector<int> used;
    for(int i = 1; i <= count; i++)
    {
      int key{};
      const std::string& name = groups[i - 1];
      if(!name.empty())
      {
        // FNV-1a of the name: the group keeps its outlet wherever it moves.
        uint32_t h = 2166136261u;
        for(unsigned char c : name)
          h = (h ^ c) * 16777619u;
        key = 20000 + int(h % 1'000'000u);
        while(std::find(used.begin(), used.end(), key) != used.end())
          key++;
      }
      else
      {
        key = 10000 + i;
      }
      used.push_back(key);
      rows.emplace_back(key, name);
    }
    return rows;
  }

  //! $1 and %1 as \1; $$ and %% as $ and %.
  static std::string toRE2Rewrite(std::string_view s)
  {
    std::string out;
    out.reserve(s.size());
    for(std::size_t i = 0; i < s.size(); i++)
    {
      const char c = s[i];
      if((c == '$' || c == '%') && i + 1 < s.size())
      {
        const char n = s[i + 1];
        if(n >= '0' && n <= '9')
        {
          out += '\\';
          out += n;
          i++;
          continue;
        }
        if(n == c)
        {
          out += c;
          i++;
          continue;
        }
      }
      out += c;
    }
    return out;
  }

  //! What the pattern is compiled from.
  struct Spec
  {
    std::string pattern;
    std::string replacement;
    Mode mode{};
    bool ignore_case{};
    bool multiline{};
  };

  //! A compiled pattern, or why there is none.
  struct Compiled
  {
    std::unique_ptr<RE2> re;
    std::string rewrite;
    std::string error;
    std::vector<re2::StringPiece> submatches;
  };

  static Compiled build(const Spec& spec)
  {
    Compiled c;
    RE2::Options opts;
    opts.set_log_errors(false);
    opts.set_case_sensitive(!spec.ignore_case);
    auto re = std::make_unique<RE2>(
        spec.multiline ? "(?m)" + spec.pattern : spec.pattern, opts);
    if(!re->ok())
    {
      c.error = re->error();
      return c;
    }

    std::string rw = toRE2Rewrite(spec.replacement);
    if(spec.mode == Replace)
    {
      if(std::string why; !re->CheckRewriteString(rw, &why))
      {
        c.error = std::move(why);
        return c;
      }
    }
    c.submatches.assign(1 + re->NumberOfCapturingGroups(), {});
    c.re = std::move(re);
    c.rewrite = std::move(rw);
    return c;
  }

  struct worker
  {
    std::function<void(Spec, uint64_t)> request;
    static std::function<void(Regex&)> work(Spec spec, uint64_t generation)
    {
      auto c = std::make_shared<Compiled>(build(spec));
      return [c = std::move(c), generation](Regex& self) mutable {
        // A later edit may have been compiled first: only the latest counts
        if(generation == self.generation)
          self.apply(std::move(*c));
      };
    }
  } worker;

  std::unique_ptr<RE2> re;
  std::string rewrite;
  std::string error_text;
  //! Error text not sent yet; a failure after success and the reverse.
  bool error_pending{};
  bool had_error{};
  std::vector<re2::StringPiece> submatches;
  std::string last_text;
  std::string text;
  uint64_t generation{};

  void setError(std::string message)
  {
    const bool failing = !message.empty();
    if(failing || had_error)
    {
      error_text = std::move(message);
      error_pending = true;
    }
    had_error = failing;
  }

  void apply(Compiled&& c)
  {
    re = std::move(c.re);
    rewrite = std::move(c.rewrite);
    submatches = std::move(c.submatches);
    setError(std::move(c.error));
  }

  //! On the audio thread, the worker compiles; anywhere else (the node being
  //! set up, a host without a worker), right away.
  void compile()
  {
    Spec spec{
        inputs.pattern.value, inputs.replacement.value, inputs.mode.value,
        inputs.ignore_case.value, inputs.multiline.value};
    ++generation;
    const auto thread = ossia::get_current_thread_type();
    if(worker.request
       && (thread == ossia::thread_type::Audio || thread == ossia::thread_type::AudioTask))
      worker.request(std::move(spec), generation);
    else
      apply(build(spec));
  }

  //! The text of a value: strings as they are, numbers printed, lists as their
  //! elements separated by spaces.
  static void appendText(std::string& out, const ossia::value& v)
  {
    switch(v.get_type())
    {
      case ossia::val_type::STRING:
        out += *v.target<std::string>();
        break;
      case ossia::val_type::INT:
        out += std::to_string(*v.target<int>());
        break;
      case ossia::val_type::FLOAT: {
        fmt::format_to(std::back_inserter(out), "{}", *v.target<float>());
        break;
      }
      case ossia::val_type::BOOL:
        out += *v.target<bool>() ? "true" : "false";
        break;
      case ossia::val_type::LIST: {
        bool first = true;
        for(auto& e : *v.target<std::vector<ossia::value>>())
        {
          if(!first)
            out += ' ';
          first = false;
          appendText(out, e);
        }
        break;
      }
      case ossia::val_type::VEC2F:
      case ossia::val_type::VEC3F:
      case ossia::val_type::VEC4F:
        appendText(out, ossia::convert<std::vector<ossia::value>>(v));
        break;
      default:
        break;
    }
  }

  //! A group as sent: its text, or the number it is when Numbers is on.
  ossia::value groupValue(re2::StringPiece s) const
  {
    std::string_view text{s.data(), s.size()};
    if(inputs.numbers.value && !text.empty())
    {
      int i{};
      if(auto [p, ec] = std::from_chars(text.data(), text.data() + text.size(), i);
         ec == std::errc{} && p == text.data() + text.size())
        return i;
      double d{};
      if(avnd_tools::value_serialization::parse_float64(text, d) && std::isfinite(d)
         && std::abs(d) <= std::numeric_limits<float>::max())
        return float(d);
    }
    return std::string{text};
  }

  static void write(ossia::value_port* port, ossia::value v, int64_t ts)
  {
    if(port)
      port->write_value(std::move(v), ts);
  }

  //! The groups of the match in `submatches`: in a list (empty strings for
  //! groups that did not take part) and each on its own outlet (nothing for
  //! those).
  std::vector<ossia::value> sendGroups(int64_t ts)
  {
    std::vector<ossia::value> groups;
    groups.reserve(submatches.size() - 1);
    for(std::size_t g = 1; g < submatches.size(); g++)
    {
      const auto& s = submatches[g];
      if(!s.data())
      {
        groups.emplace_back(std::string{});
        continue;
      }
      auto v = groupValue(s);
      if(g - 1 < outputs.captures.ports.size())
        write(outputs.captures.ports[g - 1].value, v, ts);
      groups.push_back(std::move(v));
    }
    return groups;
  }

  void process(const ossia::value& input, int64_t ts)
  {
    // The text buffers are reused from one input to the next
    if(input.target<ossia::impulse>())
    {
      text = last_text;
    }
    else
    {
      text.clear();
      appendText(text, input);
      last_text = text;
    }

    const re2::StringPiece piece{text};
    const int n = submatches.size();
    bool found = false;

    switch(inputs.mode.value)
    {
      case Search:
      case Match: {
        const auto anchor = inputs.mode.value == Match ? RE2::ANCHOR_BOTH : RE2::UNANCHORED;
        found = re->Match(piece, 0, piece.size(), anchor, submatches.data(), n);
        if(found)
        {
          write(outputs.match.value, std::string{submatches[0]}, ts);
          write(outputs.groups.value, sendGroups(ts), ts);
        }
        break;
      }
      case SearchAll:
      case Split: {
        std::vector<ossia::value> matches, groups, pieces;
        std::size_t pos = 0, last = 0;
        while(pos <= piece.size()
              && re->Match(piece, pos, piece.size(), RE2::UNANCHORED, submatches.data(), n))
        {
          found = true;
          const auto& m = submatches[0];
          const std::size_t begin = m.data() - piece.data();
          const std::size_t end = begin + m.size();
          if(inputs.mode.value == Split)
          {
            pieces.emplace_back(std::string{text.substr(last, begin - last)});
          }
          else
          {
            matches.emplace_back(std::string{m});
            groups.emplace_back(sendGroups(ts));
          }
          last = end;
          // An empty match moves on by one character, or it would match again
          pos = end > begin ? end : end + 1;
        }
        if(!found)
          break;
        if(inputs.mode.value == Split)
        {
          pieces.emplace_back(std::string{text.substr(std::min(last, text.size()))});
          write(outputs.match.value, std::move(pieces), ts);
        }
        else
        {
          write(outputs.match.value, std::move(matches), ts);
          write(outputs.groups.value, std::move(groups), ts);
        }
        break;
      }
      case Replace: {
        std::string result = text;
        found = inputs.global.value ? RE2::GlobalReplace(&result, *re, rewrite) > 0
                                    : RE2::Replace(&result, *re, rewrite);
        if(found)
          write(outputs.match.value, std::move(result), ts);
        break;
      }
    }

    write(outputs.matched.value, found, ts);
    if(!found)
      write(outputs.unmatched.value, input, ts);
  }

  void operator()(const tick& t)
  {
    const auto [start, frames] = ossia_state.timings(t);
    if(error_pending)
    {
      write(outputs.error.value, error_text, start);
      error_pending = false;
    }
    if(!inputs.input.value || !re)
      return;
    for(const auto& event : inputs.input.value->get_data())
    {
      if(event.timestamp < start || event.timestamp >= start + frames)
        continue;
      process(event.value, event.timestamp);
    }
  }
};
}
