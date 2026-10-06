#pragma once

#include <Threedim/TinyObj.hpp>
#include <boost/container/vector.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>
#include <ossia/detail/mutex.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace Threedim
{

class StrucSynth
{
public:
  halp_meta(name, "Structure Synth")
  halp_meta(category, "Visuals/Meshes")
  halp_meta(c_name, "structure_synth")
  halp_meta(description, "Generate procedural mesh geometry from an EisenScript program.")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/structure-synth.html")
  halp_meta(uuid, "bb8f3d77-4cfd-44ce-9c43-b64c54a748ab")

  struct ins
  {
    struct : halp::lineedit<"Program", "">
    {
      halp_meta(language, "eisenscript")
      void update(StrucSynth& g) { g.requestBuild(); }
    } program;

    PositionControl position;
    RotationControl rotation;
    ScaleControl scale;
    struct : halp::impulse_button<"Regenerate">
    {
      void update(StrucSynth& g) { g.requestBuild(); }
    } regen;
  } inputs;

  struct
  {
    struct : halp::mesh
    {
      halp_meta(name, "Geometry");
      halp::position_normals_geometry mesh;
    } geometry;
  } outputs;

  void operator()();

  // Programs are built on the worker, one at a time (libssynth's random
  // streams are global). Builds are numbered: the mesh of a program that was
  // replaced while it built is dropped, and the current mesh stays until the
  // latest program's arrives. A program that does not build leaves it too.
  struct worker
  {
    std::function<void(std::string, uint32_t)> request;

    static std::function<void(StrucSynth&)> work(std::string_view s, uint32_t request);
  } worker;

  void requestBuild();

  using float_vec = boost::container::vector<float, ossia::pod_allocator<float>>;
  float_vec m_vertexData;
  uint32_t m_request{};
};

}
