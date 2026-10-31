#pragma once
#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{

class ProjectorViewerFrustum
{
public:
  halp_meta(name, "Projector to Viewer Frustum")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "projector_viewer_frustum")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "The tight off-axis frustum, from the viewer, of the surface a projector lights "
      "(its XYZ map or calibration file), as the screen corners of a Camera in Screen "
      "mode (Kooima 2008, Generalized Perspective Projection), with the view and "
      "projection matrices and a render size whose pixels, seen from the viewer, are no "
      "larger than the projector's: the first pass of two-pass rendering (Raskar et al. "
      "1998, The Office of the Future, doi:10.1145/280814.280861), without a cube map.")
  halp_meta(uuid, "c3f71a0e-6d25-4b8e-9a14-2e8f5b7d0c63")

  struct ins
  {
    struct : halp::texture_input<"XYZ map", halp::rgba32f_texture>
    {
      bool request_readback{};
    } xyz;
    halp::lineedit<"Calibration file", ""> file;
    struct Viewer : halp::xyz_spinboxes_f32<"Viewer", halp::range{-1000., 1000., 0.}>
    {
      Viewer() { value = {0.f, 1.6f, 0.f}; }
    } viewer;
    halp::hslider_f32<"Plane tolerance (m)", halp::range{0., 1., 0.02}> plane_tolerance;
    halp::hslider_f32<"Oversampling", halp::range{0.25, 4., 1.}> oversampling;
    halp::hslider_f32<"Near", halp::range{0.001, 10., 0.05}> near_plane;
    halp::hslider_f32<"Far", halp::range{1., 100000., 1000.}> far_plane;
    halp::toggle<"Continuous"> continuous;
    halp::impulse_button<"Compute"> compute;
  } inputs;

  struct
  {
    halp::val_port<"Screen lower left", halp::xyz_type<float>> pa;
    halp::val_port<"Screen lower right", halp::xyz_type<float>> pb;
    halp::val_port<"Screen upper left", halp::xyz_type<float>> pc;
    //! Tangents at unit distance: left, right, bottom, top.
    halp::val_port<"Frustum", std::array<float, 4>> frustum;
    //! Column-major OpenGL view and projection.
    halp::val_port<"View", std::array<float, 16>> view;
    halp::val_port<"Projection", std::array<float, 16>> projection;
    halp::val_port<"Render size", halp::xy_type<int>> size;
    halp::val_port<"Planar", bool> planar;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
  } outputs;

  //! From a map: false and the status on failure.
  bool solve(const std::vector<float>& xyz, int w, int h);

  void operator()();

private:
  int m_wait{};
};
}
