#pragma once

#include <ossia/detail/variant.hpp>
#include <ossia/network/value/value.hpp>

#include <boost/container/static_vector.hpp>
#include <boost/mp11.hpp>
#include <boost/multi_array.hpp>

#include <halp/controls.hpp>
#include <halp/meta.hpp>

#include <algorithm>
#include <array>
#include <numeric>

namespace avnd_tools
{
inline constexpr std::size_t g_table_max_dimensions = 16;

using index_vec_type = boost::container::static_vector<int64_t, g_table_max_dimensions>;
using extent_vec_type
    = boost::container::static_vector<std::size_t, g_table_max_dimensions>;

namespace detail
{
// Generate a variant type containing multi_array<T, 1> through multi_array<T, MaxDims>
template <typename T, std::size_t MaxDims>
struct make_multi_array_variant
{
  template <std::size_t N>
  using array_type = boost::multi_array<T, N + 1>;

  using indices = boost::mp11::mp_iota_c<MaxDims>;

  template <typename I>
  using apply_array = array_type<I::value>;

  using type
      = boost::mp11::mp_rename<boost::mp11::mp_transform<apply_array, indices>,
                               ossia::slow_variant>;
};

template <typename T, std::size_t MaxDims>
using multi_array_variant_t = typename make_multi_array_variant<T, MaxDims>::type;

// Recursive element accessor: applies indices[0], indices[1], ... to get the element
template <std::size_t Dim, std::size_t CurrentIdx = 0>
struct element_accessor
{
  template <typename ArrayView, typename IndexVec>
  static auto& get(ArrayView& view, const IndexVec& indices)
  {
    if constexpr(CurrentIdx + 1 >= Dim)
    {
      return view[indices[CurrentIdx]];
    }
    else
    {
      auto sub = view[indices[CurrentIdx]];
      return element_accessor<Dim, CurrentIdx + 1>::get(sub, indices);
    }
  }

  template <typename ArrayView, typename IndexVec>
  static const auto& get(const ArrayView& view, const IndexVec& indices)
  {
    if constexpr(CurrentIdx + 1 >= Dim)
    {
      return view[indices[CurrentIdx]];
    }
    else
    {
      const auto& sub = view[indices[CurrentIdx]];
      return element_accessor<Dim, CurrentIdx + 1>::get(sub, indices);
    }
  }
};

// Bounds checker for N dimensions
template <std::size_t Dim>
struct bounds_checker
{
  template <typename Shape, typename IndexVec>
  static bool check(const Shape& shape, const IndexVec& indices)
  {
    for(std::size_t i = 0; i < Dim; ++i)
    {
      if(indices[i] < 0 || static_cast<std::size_t>(indices[i]) >= shape[i])
        return false;
    }
    return true;
  }

  template <typename IndexVec>
  static bool check_positive(const IndexVec& indices)
  {
    for(std::size_t i = 0; i < Dim; ++i)
    {
      if(indices[i] < 0 || indices[i] >= INT_MAX)
        return false;
    }
    return true;
  }
};

// Extent generator for resize operations
template <std::size_t Dim>
struct extent_generator
{
  template <typename IndexVec>
  static auto make_extents(const IndexVec& sizes)
  {
    boost::array<std::size_t, Dim> extents;
    for(std::size_t i = 0; i < Dim; ++i)
    {
      extents[i] = static_cast<std::size_t>(sizes[i]);
    }
    return extents;
  }
};

// Shape comparison for resize determination
template <std::size_t Dim>
struct needs_resize_check
{
  template <typename Shape, typename IndexVec>
  static bool check(const Shape& shape, const IndexVec& indices)
  {
    for(std::size_t i = 0; i < Dim; ++i)
    {
      if(static_cast<std::size_t>(indices[i]) >= shape[i])
        return true;
    }
    return false;
  }

  template <typename Shape, typename IndexVec>
  static auto compute_new_extents(const Shape& current_shape, const IndexVec& indices)
  {
    boost::array<std::size_t, Dim> new_extents;
    for(std::size_t i = 0; i < Dim; ++i)
    {
      new_extents[i]
          = std::max(current_shape[i], static_cast<std::size_t>(indices[i]) + 1);
    }
    return new_extents;
  }
};

template <typename T>
struct table_operations
{
  using variant_type = multi_array_variant_t<T, g_table_max_dimensions>;

  template <typename OutputPort>
  static bool read(variant_type& buffer, const index_vec_type& cursor, OutputPort& output)
  {
    return ossia::visit(
        [&](auto& arr) -> bool {
          constexpr std::size_t Dim = std::remove_reference_t<decltype(arr)>::dimensionality;

          if(cursor.size() != Dim)
            return false;

          if(arr.num_elements() == 0)
            return false;

          const auto* shape = arr.shape();
          if(!bounds_checker<Dim>::check(shape, cursor))
            return false;

          output.value = element_accessor<Dim>::get(arr, cursor);
          return true;
        },
        buffer);
  }

  static void set(variant_type& buffer, const index_vec_type& cursor, const T& value)
  {
    ossia::visit(
        [&](auto& arr) {
          constexpr std::size_t Dim = std::remove_reference_t<decltype(arr)>::dimensionality;

          if(cursor.size() != Dim)
            return;

          if(!bounds_checker<Dim>::check_positive(cursor))
            return;

          const auto* shape = arr.shape();

          if(arr.num_elements() == 0 || needs_resize_check<Dim>::check(shape, cursor))
          {
            // Compute new extents
            boost::array<std::size_t, Dim> new_extents;
            if(arr.num_elements() == 0)
            {
              for(std::size_t i = 0; i < Dim; ++i)
                new_extents[i] = static_cast<std::size_t>(cursor[i]) + 1;
            }
            else
            {
              new_extents = needs_resize_check<Dim>::compute_new_extents(shape, cursor);
            }
            arr.resize(new_extents);
          }

          element_accessor<Dim>::get(arr, cursor) = value;
        },
        buffer);
  }

  static extent_vec_type get_shape(const variant_type& buffer)
  {
    return ossia::visit(
        [](const auto& arr) -> extent_vec_type {
          constexpr std::size_t Dim = std::remove_reference_t<decltype(arr)>::dimensionality;
          extent_vec_type result;
          const auto* shape = arr.shape();
          for(std::size_t i = 0; i < Dim; ++i)
            result.push_back(shape[i]);
          return result;
        },
        buffer);
  }

  static std::size_t num_elements(const variant_type& buffer)
  {
    return ossia::visit([](const auto& arr) { return arr.num_elements(); }, buffer);
  }

  static void clear(variant_type& buffer)
  {
    ossia::visit(
        [](auto& arr) {
          constexpr std::size_t Dim = std::remove_reference_t<decltype(arr)>::dimensionality;
          boost::array<std::size_t, Dim> zero_extents;
          zero_extents.fill(0);
          arr.resize(zero_extents);
        },
        buffer);
  }

  static void resize(variant_type& buffer, const extent_vec_type& extents)
  {
    ossia::visit(
        [&](auto& arr) {
          constexpr std::size_t Dim = std::remove_reference_t<decltype(arr)>::dimensionality;

          if(extents.size() != Dim)
            return;

          boost::array<std::size_t, Dim> new_extents;
          for(std::size_t i = 0; i < Dim; ++i)
            new_extents[i] = extents[i];
          arr.resize(new_extents);
        },
        buffer);
  }

  static void fill(variant_type& buffer, const T& value)
  {
    ossia::visit(
        [&](auto& arr) { std::fill(arr.data(), arr.data() + arr.num_elements(), value); },
        buffer);
  }

  static std::vector<T> to_vector(const variant_type& buffer)
  {
    return ossia::visit(
        [](const auto& arr) -> std::vector<T> {
          return std::vector<T>(arr.data(), arr.data() + arr.num_elements());
        },
        buffer);
  }

  // Dump the array as nested vectors: vector<vector<...vector<T>...>>
  // Returns ossia::value containing the nested structure
  static ossia::value dump(const variant_type& buffer)
  {
    return ossia::visit(
        [](const auto& arr) -> ossia::value {
          constexpr std::size_t Dim
              = std::remove_reference_t<decltype(arr)>::dimensionality;
          return dump_impl<Dim>(arr);
        },
        buffer);
  }

private:
  // Recursive dump implementation
  template <std::size_t Dim, typename Array>
  static ossia::value dump_impl(const Array& arr)
  {
    const auto* shape = arr.shape();

    if constexpr(Dim == 1)
    {
      // Base case: 1D array -> vector of values
      std::vector<ossia::value> result;
      result.reserve(shape[0]);
      for(std::size_t i = 0; i < shape[0]; ++i)
        result.push_back(arr[i]);
      return result;
    }
    else
    {
      // Recursive case: N-D array -> vector of (N-1)-D dumps
      std::vector<ossia::value> result;
      result.reserve(shape[0]);
      for(std::size_t i = 0; i < shape[0]; ++i)
      {
        result.push_back(dump_impl<Dim - 1>(arr[i]));
      }
      return result;
    }
  }
};

template <typename T>
struct dimension_changer
{
  using variant_type = multi_array_variant_t<T, g_table_max_dimensions>;

  static void change_dimensions(variant_type& buffer, std::size_t new_dims)
  {
    if(new_dims < 1 || new_dims > g_table_max_dimensions)
      return;

    // Use mp_with_index to dispatch to the correct dimension at compile time
    boost::mp11::mp_with_index<g_table_max_dimensions>(new_dims - 1, [&](auto I) {
      constexpr std::size_t Dim = I.value + 1;
      using new_array_type = boost::multi_array<T, Dim>;

      // Create empty array with the new dimensionality
      new_array_type new_arr;
      buffer = std::move(new_arr);
    });
  }

  static std::size_t current_dimensions(const variant_type& buffer)
  {
    return ossia::visit(
        [](const auto& arr) -> std::size_t {
          return std::remove_reference_t<decltype(arr)>::dimensionality;
        },
        buffer);
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

  detail::multi_array_variant_t<value_type, g_table_max_dimensions> buffer;

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
