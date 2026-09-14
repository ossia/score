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

//! Does the object declare any of the renderer-side entry points score calls
//! on the object it gives a renderer? Such an object holds state that belongs
//! to one RenderList and cannot be shared between them.
//!
//! Detection is by address-of on the member: an object hiding one of these
//! behind a template or an overload set reads as "no hook" here.
template <typename T>
concept has_renderer_state
    = requires(T& t) { t.renderlist; } || requires { &T::init; }
      || requires { &T::update; } || requires { &T::release; }
      || requires { &T::runInitialPasses; } || requires { &T::runRenderPass; }
      || requires { &T::inputAboutToFinish; };

//! An object that feeds the gfx graph without ever touching the RHI: its only
//! gfx ports are CPU buffer outputs, which the renderers upload for it.
//!
//! Everything a renderer owns for it (the QRhiBuffer, its resource updates)
//! is renderer side, and nothing in the object is - while the object itself
//! has a CPU identity that must not be duplicated (e.g. one TCP listener per
//! object). One instance per RenderList would mean one per output window, and
//! rebuilding a renderer would restart it. It gets a single instance shared
//! by every renderer of the node instead (GfxNode::rendererState).
//!
//! This is only about the object's *instances*: such a node still lives in
//! the gfx graph (is_gpu), because a buffer output has nowhere else to go.
template <typename T>
concept CpuOnlyBufferNode
    = avnd::cpu_buffer_output_introspection<T>::size > 0
      && avnd::gpu_buffer_output_introspection<T>::size == 0
      && avnd::buffer_input_introspection<T>::size == 0
      && avnd::texture_input_introspection<T>::size == 0
      && avnd::texture_output_introspection<T>::size == 0
      && avnd::geometry_input_introspection<T>::size == 0
      && avnd::geometry_output_introspection<T>::size == 0
      && scene_input_introspection<T>::size == 0
      && scene_output_introspection<T>::size == 0
      && avnd::gpu_render_target_output_port_output_introspection<T>::size == 0
      && !GpuGraphicsNode2<T> && !GpuComputeNode2<T> && !has_renderer_state<T>;

template <typename T>
concept has_ossia_layer = requires { sizeof(typename T::Layer); };
}
