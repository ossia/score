#pragma once

#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <boost/container/static_vector.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace avnd_tools
{

inline constexpr std::size_t g_table_max_dimensions = 16;

using index_vec_type = boost::container::static_vector<int64_t, g_table_max_dimensions>;
using extent_vec_type
    = boost::container::static_vector<std::size_t, g_table_max_dimensions>;

namespace detail
{
template <typename T>
struct table_buffer
{
  std::size_t dimensions{1};
  extent_vec_type extents{0};
  std::vector<T> values;
};

template <typename T>
struct table_operations
{
  using buffer_type = table_buffer<T>;

  static std::optional<std::size_t>
  element_count(const extent_vec_type& extents)
  {
    std::size_t count = 1;
    for(const std::size_t extent : extents)
    {
      if(extent == 0)
        return 0;
      if(count > std::numeric_limits<std::size_t>::max() / extent)
        return std::nullopt;
      count *= extent;
    }
    return count;
  }

  static std::optional<std::size_t>
  flat_index(const buffer_type& buffer, const index_vec_type& cursor)
  {
    if(cursor.size() != buffer.dimensions)
      return std::nullopt;

    std::size_t result = 0;
    for(std::size_t i = 0; i < buffer.dimensions; ++i)
    {
      if(cursor[i] < 0
         || static_cast<std::size_t>(cursor[i]) >= buffer.extents[i])
        return std::nullopt;
      result = result * buffer.extents[i] + static_cast<std::size_t>(cursor[i]);
    }
    return result;
  }

  template <typename OutputPort>
  static bool
  read(const buffer_type& buffer, const index_vec_type& cursor, OutputPort& output)
  {
    const auto index = flat_index(buffer, cursor);
    if(!index)
      return false;
    output.value = buffer.values[*index];
    return true;
  }

  static void set(buffer_type& buffer, const index_vec_type& cursor, const T& value)
  {
    if(cursor.size() != buffer.dimensions)
      return;

    extent_vec_type new_extents = buffer.extents;
    bool needs_resize = false;
    for(std::size_t i = 0; i < buffer.dimensions; ++i)
    {
      if(cursor[i] < 0
         || cursor[i] >= std::numeric_limits<int>::max())
        return;

      const std::size_t required = static_cast<std::size_t>(cursor[i]) + 1;
      if(required > new_extents[i])
      {
        new_extents[i] = required;
        needs_resize = true;
      }
    }

    if(needs_resize)
      resize(buffer, new_extents);

    if(const auto index = flat_index(buffer, cursor))
      buffer.values[*index] = value;
  }

  static extent_vec_type get_shape(const buffer_type& buffer)
  {
    return buffer.extents;
  }

  static std::size_t num_elements(const buffer_type& buffer)
  {
    return buffer.values.size();
  }

  static void clear(buffer_type& buffer)
  {
    buffer.extents.assign(buffer.dimensions, 0);
    buffer.values.clear();
  }

  static void resize(buffer_type& buffer, const extent_vec_type& extents)
  {
    if(extents.size() != buffer.dimensions || extents == buffer.extents)
      return;

    const auto count = element_count(extents);
    if(!count)
      return;

    std::vector<T> new_values(*count);
    index_vec_type coordinates(buffer.dimensions, 0);
    for(std::size_t old_index = 0; old_index < buffer.values.size(); ++old_index)
    {
      std::size_t remainder = old_index;
      bool preserved = true;
      for(std::size_t i = buffer.dimensions; i-- > 0;)
      {
        coordinates[i] = static_cast<int64_t>(remainder % buffer.extents[i]);
        remainder /= buffer.extents[i];
        if(static_cast<std::size_t>(coordinates[i]) >= extents[i])
          preserved = false;
      }

      if(preserved)
      {
        std::size_t new_index = 0;
        for(std::size_t i = 0; i < buffer.dimensions; ++i)
          new_index = new_index * extents[i]
                      + static_cast<std::size_t>(coordinates[i]);
        new_values[new_index] = std::move(buffer.values[old_index]);
      }
    }

    buffer.extents = extents;
    buffer.values = std::move(new_values);
  }

  static void fill(buffer_type& buffer, const T& value)
  {
    std::fill(buffer.values.begin(), buffer.values.end(), value);
  }

  static ossia::value dump(const buffer_type& buffer)
  {
    return dump_impl(buffer, 0, 0);
  }

private:
  static ossia::value
  dump_impl(const buffer_type& buffer, std::size_t dimension, std::size_t offset)
  {
    std::vector<ossia::value> result;
    result.reserve(buffer.extents[dimension]);

    if(dimension + 1 == buffer.dimensions)
    {
      for(std::size_t i = 0; i < buffer.extents[dimension]; ++i)
        result.push_back(buffer.values[offset + i]);
    }
    else
    {
      std::size_t stride = 1;
      for(std::size_t i = dimension + 1; i < buffer.dimensions; ++i)
        stride *= buffer.extents[i];

      for(std::size_t i = 0; i < buffer.extents[dimension]; ++i)
        result.push_back(dump_impl(buffer, dimension + 1, offset + i * stride));
    }
    return result;
  }
};

template <typename T>
struct dimension_changer
{
  using buffer_type = table_buffer<T>;

  static void change_dimensions(buffer_type& buffer, std::size_t new_dims)
  {
    if(new_dims < 1 || new_dims > g_table_max_dimensions)
      return;

    buffer.dimensions = new_dims;
    buffer.extents.assign(new_dims, 0);
    buffer.values.clear();
  }

  static std::size_t current_dimensions(const buffer_type& buffer)
  {
    return buffer.dimensions;
  }
};
}

struct Table
{
  halp_meta(name, "Table")
  halp_meta(author, "ossia team")
  halp_meta(category, "Control/Data processing")
  halp_meta(description, "Store arbitrary data in an N-dimensional table (1-16D)")
  halp_meta(c_name, "avnd_table_nd")
  halp_meta(uuid, "98418d3a-58c3-4d1f-b716-83c0988174c3")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/table.html")

  using value_type = ossia::value;
  using ops = detail::table_operations<value_type>;
  using dim_changer = detail::dimension_changer<value_type>;

  struct
  {
    struct : halp::val_port<"Read", std::optional<index_vec_type>>
    {
      void update(Table& t)
      {
        if(value)
        {
          if(ops::read(t.buffer, *value, t.outputs.output))
            return;
        }
        t.outputs.output.value = ossia::value{};
      }
    } read;

    struct : halp::val_port<"Set cell", std::vector<ossia::value>>
    {
      void update(Table& t)
      {
        // Need at least indices + value
        if(value.size() < 2)
          return;

        const std::size_t dims = dim_changer::current_dimensions(t.buffer);
        if(value.size() != dims + 1)
          return;

        index_vec_type indices;
        for(std::size_t i = 0; i < dims; i++)
          indices.push_back(ossia::convert<int>(value[i]));

        ops::set(t.buffer, indices, value.back());
      }
    } set;

    struct : halp::val_port<"Clear cell", index_vec_type>
    {
      void update(Table& t) { t.clear_cell(value); }
    } clear_cell;

    struct : halp::val_port<"Resize", extent_vec_type>
    {
      void update(Table& t) { t.resize(value); }
    } resize;

    struct : halp::val_port<"Fill", ossia::value>
    {
      void update(Table& t) { ops::fill(t.buffer, value); }
    } fill;

    struct : halp::spinbox_i32<"Dimensions", halp::range{1, 16, 1}>
    {
      void update(Table& t)
      {
        if(value >= 1 && value <= 16)
        {
          const std::size_t current = dim_changer::current_dimensions(t.buffer);
          if(static_cast<std::size_t>(value) != current)
          {
            dim_changer::change_dimensions(t.buffer, value);
          }
        }
      }
    } dims;
    halp::maintained_button<"Clear"> clear;
    halp::maintained_button<"Lock"> lock;
    struct : halp::impulse_button<"Dump">
    {
      void update(Table& t) { t.outputs.output.value = ops::dump(t.buffer); }
    } dump;
    halp::toggle<"Preserve"> preserve;

  } inputs;

  struct
  {
    halp::val_port<"Output", ossia::value> output;
    halp::val_port<"Shape", extent_vec_type> shape;
    halp::val_port<"Size", int64_t> size;
  } outputs;

  detail::table_buffer<value_type> buffer;

  void clear_cell(const index_vec_type& indices)
  {
    ops::set(buffer, indices, ossia::value{});
  }

  void resize(const extent_vec_type& extents)
  {
    const std::size_t current_dims = dim_changer::current_dimensions(buffer);
    if(extents.size() != current_dims)
      return;
    ops::resize(buffer, extents);
  }

  void operator()()
  {
    if(inputs.clear)
    {
      ops::clear(buffer);
    }

    // Update output ports with current state
    auto shape = ops::get_shape(buffer);
    outputs.shape.value.clear();
    for(auto s : shape)
      outputs.shape.value.push_back(static_cast<int64_t>(s));

    outputs.size.value = static_cast<int64_t>(ops::num_elements(buffer));
  }
};

}
