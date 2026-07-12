#pragma once

#include <Crousti/Concepts.hpp>
#include <Crousti/SceneConcepts.hpp>

#include <ossia/dataflow/safe_nodes/port.hpp>

#include <boost/mp11/algorithm.hpp>

#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
namespace oscr
{
template <typename T>
concept GpuNode
    = avnd::texture_input_introspection<T>::size > 0
      || avnd::texture_output_introspection<T>::size > 0
      || avnd::buffer_input_introspection<T>::size > 0
      || avnd::buffer_output_introspection<T>::size > 0
      || avnd::geometry_input_introspection<T>::size > 0
      || avnd::geometry_output_introspection<T>::size > 0
      || scene_input_introspection<T>::size > 0
      || scene_output_introspection<T>::size > 0
      || avnd::gpu_render_target_output_port_output_introspection<T>::size > 0;

// Halp shader nodes (vertex+fragment / compute) currently route through
// CustomGpuRenderer / GpuComputeRenderer, neither of which carries
// geometry_ / scene_ I/O storage today. Exclude nodes that declare those
// ports from the GpuGraphicsNode2 / GpuComputeNode2 dispatch so they fall
// through to GfxNode<> (which has the proper storage via CpuFilterNode /
// CpuAnalysisNode). When CustomGpuRenderer / GpuComputeRenderer gain
// dedicated scene_ / geometry_ storage, drop the requires-clause exclusion
// here and add init_input + readInput / upload paths in those renderers.
template <typename T>
concept GpuGraphicsNode2
    = requires { T::layout::graphics; }
      && (avnd::geometry_input_introspection<T>::size == 0)
      && (avnd::geometry_output_introspection<T>::size == 0)
      && (scene_input_introspection<T>::size == 0)
      && (scene_output_introspection<T>::size == 0);

template <typename T>
concept GpuComputeNode2
    = requires { T::layout::compute; }
      && (avnd::geometry_input_introspection<T>::size == 0)
      && (avnd::geometry_output_introspection<T>::size == 0)
      && (scene_input_introspection<T>::size == 0)
      && (scene_output_introspection<T>::size == 0);

template <typename T>
concept is_gpu = GpuNode<T> || GpuGraphicsNode2<T> || GpuComputeNode2<T>;

template <typename T>
concept has_ossia_layer = requires { sizeof(typename T::Layer); };
}
