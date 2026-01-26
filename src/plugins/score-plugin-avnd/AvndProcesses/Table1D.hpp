#pragma once

#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <vector>

namespace avnd_tools
{

struct Table1D
{
  halp_meta(name, "Table (1D)")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Store arbitrary data in a 1-dimensional table")
  halp_meta(c_name, "avnd_table_1d")
  halp_meta(uuid, "a7b3c1d2-4e5f-6789-abcd-ef0123456781")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/table.html")

  using value_type = ossia::value;

  struct
  {
    struct : halp::val_port<"Read", std::optional<int64_t>>
    {
      void update(Table1D& t)
      {
        if(value)
        {
          if(t.read(*value))
            return;
        }
        t.outputs.output.value = ossia::value{};
      }
    } read;

    struct : halp::val_port<"Set", std::pair<int64_t, ossia::value>>
    {
      void update(Table1D& t) { t.set(value.first, value.second); }
    } set;

    struct : halp::val_port<"Prepend", ossia::value>
    {
      void update(Table1D& t) { t.prepend(value); }
    } prepend;

    struct : halp::val_port<"Append", ossia::value>
    {
      void update(Table1D& t) { t.append(value); }
    } append;

    struct : halp::val_port<"Insert", std::pair<int64_t, ossia::value>>
    {
      void update(Table1D& t) { t.insert(value.first, value.second); }
    } insert;

    struct : halp::val_port<"Erase", int64_t>
    {
      void update(Table1D& t) { t.erase(value); }
    } erase;

    struct : halp::val_port<"Pop front", bool>
    {
      void update(Table1D& t)
      {
        if(value)
          t.pop_front();
      }
    } pop_front;

    struct : halp::val_port<"Pop back", bool>
    {
      void update(Table1D& t)
      {
        if(value)
          t.pop_back();
      }
    } pop_back;

    struct : halp::val_port<"Resize", int64_t>
    {
      void update(Table1D& t) { t.resize(value); }
    } resize;

    struct : halp::val_port<"Fill", ossia::value>
    {
      void update(Table1D& t) { t.fill(value); }
    } fill_port;

    halp::maintained_button<"Clear"> clear;
    halp::maintained_button<"Lock"> lock;
    struct : halp::impulse_button<"Dump">
    {
      void update(Table1D& t) { t.outputs.output.value = t.buffer; }
    } dump;
    halp::toggle<"Preserve"> preserve;

  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
    halp::val_port<"Front", ossia::value> front;
    halp::val_port<"Back", ossia::value> back;
    halp::val_port<"Size", int64_t> size;
    halp::val_port<"All", std::vector<ossia::value>> all;
  } outputs;

  std::vector<value_type> buffer;

  bool read(int64_t index)
  {
    if(index < 0)
      return false;

    const std::size_t idx = static_cast<std::size_t>(index);
    if(idx >= buffer.size())
      return false;

    outputs.output.value = buffer[idx];
    return true;
  }

  void set(int64_t index, const value_type& value)
  {
    if(index < 0 || index >= INT_MAX)
      return;

    const std::size_t idx = static_cast<std::size_t>(index);

    if(idx >= buffer.size())
      buffer.resize(idx + 1);

    buffer[idx] = value;
  }

  void prepend(const value_type& value) { buffer.insert(buffer.begin(), value); }

  void append(const value_type& value) { buffer.push_back(value); }

  void insert(int64_t index, const value_type& value)
  {
    if(index < 0)
      return;

    const std::size_t idx = static_cast<std::size_t>(index);

    if(idx >= buffer.size())
    {
      set(index, value);
      return;
    }

    buffer.insert(buffer.begin() + idx, value);
  }

  void erase(int64_t index)
  {
    if(index < 0)
      return;

    const std::size_t idx = static_cast<std::size_t>(index);

    if(idx >= buffer.size())
      return;

    buffer.erase(buffer.begin() + idx);
  }

  void pop_front()
  {
    if(buffer.empty())
      return;
    buffer.erase(buffer.begin());
  }

  void pop_back()
  {
    if(buffer.empty())
      return;
    buffer.pop_back();
  }

  void resize(int64_t new_size)
  {
    if(new_size < 0)
      return;
    buffer.resize(static_cast<std::size_t>(new_size));
  }

  void fill(const value_type& value) { std::fill(buffer.begin(), buffer.end(), value); }

  void do_clear() { buffer.clear(); }

  void operator()()
  {
    if(inputs.clear)
    {
      do_clear();
    }

    const std::size_t sz = buffer.size();

    outputs.size.value = static_cast<int64_t>(sz);

    if(sz > 0)
    {
      outputs.front.value = buffer.front();
      outputs.back.value = buffer.back();
    }
    else
    {
      outputs.front.value = ossia::value{};
      outputs.back.value = ossia::value{};
    }

    outputs.all.value = buffer;
  }
};

} // namespace avnd_tools
