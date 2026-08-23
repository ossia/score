#include "ValuePrettyPrint.hpp"

#include <ossia/network/value/format_value.hpp>

#include <algorithm>
#include <iterator>

namespace State
{
namespace
{
using out_it = std::back_insert_iterator<std::string>;

bool isContainer(const ossia::value& v) noexcept
{
  switch(v.get_type())
  {
    case ossia::val_type::LIST:
    case ossia::val_type::MAP:
      return true;
    default:
      return false;
  }
}

bool isFlat(const std::vector<ossia::value>& l) noexcept
{
  return std::none_of(l.begin(), l.end(), isContainer);
}

bool isFlat(const ossia::value_map_type& m) noexcept
{
  return std::none_of(
      m.begin(), m.end(), [](const auto& kv) { return isContainer(kv.second); });
}

void newline(std::string& out, int depth, const PrettyPrintOptions& o)
{
  out.push_back('\n');
  if(depth > 0 && o.indent > 0)
    out.append(std::size_t(depth) * o.indent, ' ');
}

struct Printer
{
  std::string& out;
  const PrettyPrintOptions& o;

  void scalar(const ossia::value& v) { fmt::format_to(out_it{out}, "{}", v); }

  void key(const std::string& k) { fmt::format_to(out_it{out}, "\"{}\": ", k); }

  void value(const ossia::value& v, int depth)
  {
    switch(v.get_type())
    {
      case ossia::val_type::LIST:
        list(*v.target<std::vector<ossia::value>>(), depth);
        break;
      case ossia::val_type::MAP:
        map(*v.target<ossia::value_map_type>(), depth);
        break;
      default:
        scalar(v);
        break;
    }
  }

  // Elements of a flat container, either all on one line or wrapped a few per
  // line. `each` writes one element.
  template <typename Range, typename Each>
  void flat(const Range& r, int depth, Each&& each)
  {
    const auto n = std::ssize(r);
    const bool wrap = n > o.maxInlineElements && o.elementsPerLine > 0;
    if(wrap)
      newline(out, depth + 1, o);

    std::ptrdiff_t i = 0;
    for(const auto& e : r)
    {
      each(e);
      if(++i < n)
      {
        out.push_back(',');
        if(wrap && (i % o.elementsPerLine) == 0)
          newline(out, depth + 1, o);
        else
          out.push_back(' ');
      }
    }

    if(wrap)
      newline(out, depth, o);
  }

  // Elements of a nested container: one per line, a level deeper.
  template <typename Range, typename Each>
  void nested(const Range& r, int depth, Each&& each)
  {
    const auto n = std::ssize(r);
    std::ptrdiff_t i = 0;
    for(const auto& e : r)
    {
      newline(out, depth + 1, o);
      each(e);
      if(++i < n)
        out.push_back(',');
    }
    newline(out, depth, o);
  }

  void list(const std::vector<ossia::value>& l, int depth)
  {
    out.append("list: [");
    if(!l.empty())
    {
      if(isFlat(l))
        flat(l, depth, [this](const ossia::value& e) { scalar(e); });
      else
        nested(l, depth, [this, depth](const ossia::value& e) { value(e, depth + 1); });
    }
    out.push_back(']');
  }

  void map(const ossia::value_map_type& m, int depth)
  {
    out.append("map: {");
    if(!m.empty())
    {
      if(isFlat(m))
        flat(m, depth, [this](const auto& kv) {
          key(kv.first);
          scalar(kv.second);
        });
      else
        nested(m, depth, [this, depth](const auto& kv) {
          key(kv.first);
          value(kv.second, depth + 1);
        });
    }
    out.push_back('}');
  }
};
}

void prettyPrintValue(
    std::string& out, const ossia::value& v, int depth, const PrettyPrintOptions& opts)
{
  Printer{out, opts}.value(v, depth);
}

void printValue(std::string& out, const ossia::value& v)
{
  fmt::format_to(out_it{out}, "{}", v);
}
}
