#pragma once

#include <Crousti/Concepts.hpp>

#include <ossia/dataflow/safe_nodes/port.hpp>

#include <boost/mp11/algorithm.hpp>
#include <boost/pfr.hpp>

#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
namespace oscr
{
template <typename T>
concept GpuNode = avnd::texture_input_introspection<T>::size > 0
                  || avnd::texture_output_introspection<T>::size > 0
                  || avnd::buffer_input_introspection<T>::size > 0
                  || avnd::buffer_output_introspection<T>::size > 0
                  || avnd::geometry_input_introspection<T>::size > 0
                  || avnd::geometry_output_introspection<T>::size > 0;

template <typename T>
concept GpuGraphicsNode2 = requires
{
  T::layout::graphics;
};

template <typename T>
concept GpuComputeNode2 = requires
{
  T::layout::compute;
};

template <typename T>
concept is_gpu = GpuNode<T> || GpuGraphicsNode2<T> || GpuComputeNode2<T>;

template <typename T>
concept has_ossia_layer = requires { sizeof(typename T::Layer); };
}
