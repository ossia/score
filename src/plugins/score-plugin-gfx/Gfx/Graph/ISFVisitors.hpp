#pragma once
#include <isf.hpp>

#include <ossia/detail/variant.hpp>

namespace score::gfx
{
// ---------------------------------------------------------------------------
// Descriptor port walker (diagnostic 097, refactor R3)
// ---------------------------------------------------------------------------
//
// SINGLE source of truth for "how many input ports / output ports / samplers
// does each desc.inputs entry produce?". Every prior call site (CSF
// port_indices, RawRaster port_idx, RawRaster bindAuxTexturesInit, ISF
// IsfBindingsBuilder) had its own copy of this rule — and they had drifted
// (e.g. CSF over-counted inlets for write-only storage_input without a
// flex-array sizing field; IsfBindingsBuilder added a phantom inlet for every
// write-only csf_image_input). Mirrors `isf_input_port_vis` in ISFNode.cpp,
// which is the actual port-creation code.
//
// When a new isf::*_input variant is added, update isf_input_port_vis AND
// the matching `operator()` here — keep them in lockstep.
struct port_counts
{
  int inlets{};   //!< score input ports created by this desc.inputs entry
  int outlets{};  //!< score output ports created
  int samplers{}; //!< sampler slots in initInputSamplers (1 per image-like;
                  //!< +1 for image_input.depth on a non-GrabsFromSource port)

  port_counts& operator+=(const port_counts& o) noexcept
  {
    inlets += o.inlets;
    outlets += o.outlets;
    samplers += o.samplers;
    return *this;
  }
};

// Returns the port_counts contributed by a single input variant. Mirrors
// isf_input_port_vis (ISFNode.cpp) one-to-one.
struct isf_input_port_count_vis
{
  port_counts operator()(const isf::float_input&) const noexcept   { return {1, 0, 0}; }
  port_counts operator()(const isf::long_input&) const noexcept    { return {1, 0, 0}; }
  port_counts operator()(const isf::event_input&) const noexcept   { return {1, 0, 0}; }
  port_counts operator()(const isf::bool_input&) const noexcept    { return {1, 0, 0}; }
  port_counts operator()(const isf::point2d_input&) const noexcept { return {1, 0, 0}; }
  port_counts operator()(const isf::point3d_input&) const noexcept { return {1, 0, 0}; }
  port_counts operator()(const isf::color_input&) const noexcept   { return {1, 0, 0}; }
  port_counts operator()(const isf::audio_input&) const noexcept    { return {1, 0, 0}; }
  port_counts operator()(const isf::audioHist_input&) const noexcept{ return {1, 0, 0}; }
  port_counts operator()(const isf::audioFFT_input&) const noexcept { return {1, 0, 0}; }

  port_counts operator()(const isf::image_input& in) const noexcept
  {
    // GrabsFromSource means no own render target → the matching depth sampler
    // (image_input.depth==true) is also NOT created in initInputSamplers.
    const bool grabs = (in.dimensions == 3 || in.is_array || in.is_static);
    const int extra_depth_sampler = (in.depth && !grabs) ? 1 : 0;
    return {1, 0, 1 + extra_depth_sampler};
  }
  port_counts operator()(const isf::cubemap_input&) const noexcept { return {1, 0, 1}; }
  port_counts operator()(const isf::texture_input&) const noexcept { return {1, 0, 1}; }

  port_counts operator()(const isf::storage_input& in) const noexcept
  {
    // read_only: 1 input port (no output, no sampler).
    // write/read_write: 1 output port; +1 input port if the layout's last
    //   field is a flexible array (synthesized long_input for sizing).
    if(in.access == "read_only")
      return {1, 0, 0};
    port_counts c{0, 1, 0};
    if(!in.layout.empty()
       && in.layout.back().type.find("[]") != std::string::npos)
      c.inlets = 1;
    return c;
  }

  port_counts operator()(const isf::uniform_input&) const noexcept
  {
    return {1, 0, 0};
  }

  port_counts operator()(const isf::csf_image_input& in) const noexcept
  {
    // read_only: 1 input port; write/read_write: 1 output port (no input).
    if(in.access == "read_only")
      return {1, 0, 0};
    return {0, 1, 0};
  }

  port_counts operator()(const isf::geometry_input& in) const noexcept
  {
    port_counts c{};
    if(in.attributes.empty())
    {
      // Pass-through: 1 inlet + 1 outlet
      c.inlets = 1;
      c.outlets = 1;
    }
    else
    {
      for(const auto& attr : in.attributes)
        if(attr.access == "read_only" || attr.access == "read_write")
        { c.inlets = 1; break; }
      for(const auto& attr : in.attributes)
        if(attr.access == "write_only" || attr.access == "read_write")
        { c.outlets = 1; break; }
    }
    // $USER ports → synthesized long_input each (1 inlet)
    if(in.vertex_count.find("$USER") != std::string::npos)   c.inlets++;
    if(in.instance_count.find("$USER") != std::string::npos) c.inlets++;
    for(const auto& aux : in.auxiliary)
      if(aux.size.find("$USER") != std::string::npos)
        c.inlets++;
    return c;
  }
};

// Walk desc.inputs once. For each input, the visitor receives:
//   - the isf::input entry
//   - the cumulative port_counts BEFORE this input (so cur.inlets is the
//     index of the first input port this entry creates, if any)
//   - the per-input port_counts delta (how many ports this entry creates)
// Cumulative state is then advanced before moving on.
//
// Callers needing a non-zero starting offset (e.g. RawRaster's port 0 is
// the implicit Geometry input) can pass it in `start` — its inlets/outlets
// are accumulated upfront.
template <typename F>
inline void walk_descriptor_inputs(
    const isf::descriptor& desc, port_counts start, F&& fn)
{
  port_counts cur = start;
  for(const auto& inp : desc.inputs)
  {
    port_counts delta = ossia::visit(isf_input_port_count_vis{}, inp.data);
    fn(inp, cur, delta);
    cur += delta;
  }
}

// Convenience overload: zero starting offset.
template <typename F>
inline void walk_descriptor_inputs(const isf::descriptor& desc, F&& fn)
{
  walk_descriptor_inputs(desc, port_counts{}, std::forward<F>(fn));
}

struct isf_input_size_vis
{
  int sz{};
  void operator()(const isf::float_input&) noexcept { sz += 4; }

  void operator()(const isf::long_input&) noexcept { sz += 4; }

  void operator()(const isf::event_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::bool_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::point2d_input&) noexcept
  {
    if(sz % 8 != 0)
      sz += 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input&) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 3 * 4;
  }

  void operator()(const isf::color_input&) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 4 * 4;
  }

  void operator()(const isf::image_input&) noexcept { }
  void operator()(const isf::cubemap_input&) noexcept { }

  void operator()(const isf::audio_input&) noexcept { }
  void operator()(const isf::audioFFT_input&) noexcept { }
  void operator()(const isf::audioHist_input&) noexcept { }

  // CSF-specific input handlers
  void operator()(const isf::storage_input& in) noexcept
  {
    // Must match what isf_input_port_vis (ISFNode.cpp) actually writes into the
    // blob — and the synthesized "size" int it creates: ONLY a writable buffer
    // whose layout ends in a flexible-array member. Reserving for every write
    // buffer over-allocated the UBO (harmless, but desynced from the port
    // visitor and the generated GLSL Params/material_t block).
    if(in.access.contains("write") && !in.layout.empty()
       && in.layout.back().type.find("[]") != std::string::npos)
    {
      (*this)(isf::long_input{});
    }
  }

  void operator()(const isf::uniform_input&) noexcept
  {
    // UBO inputs are bound from an upstream Buffer port; they do not
    // contribute to the material UBO size.
  }

  void operator()(const isf::texture_input in) noexcept { }

  void operator()(const isf::csf_image_input& in) noexcept
  {
    // isf_input_port_vis does NOT write anything into the material blob for
    // write csf_image inputs (its point2d/long synthesis is commented out), so
    // reserve nothing here — keep the size visitor and the port visitor (and
    // hence the generated uniform block) in agreement.
  }

  void operator()(const isf::geometry_input& in) noexcept
  {
    // Geometry inputs don't contribute to the material UBO size.
    // Their buffers are bound separately as SSBOs.
    // But $USER ports create long_input (int) entries in the material UBO.
    if(in.vertex_count.find("$USER") != std::string::npos)
      (*this)(isf::long_input{});
    if(in.instance_count.find("$USER") != std::string::npos)
      (*this)(isf::long_input{});
    for(const auto& aux : in.auxiliary)
      if(aux.size.find("$USER") != std::string::npos)
        (*this)(isf::long_input{});
  }
};
}
