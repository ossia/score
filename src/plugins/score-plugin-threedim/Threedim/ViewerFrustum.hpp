#pragma once
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CalibrationJobs.hpp>
#include <Threedim/FloatTextureIO.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
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
      "1998, The Office of the Future, doi:10.1145/280814.280861), without a cube map. "
      "The matrices are OpenGL's (view z = -near at NDC z = -1): Pose matrix and "
      "Projection matrix are written row by row, as a Camera in Matrix mode reads them; "
      "View and Projection are the same matrices column-major.")
  halp_meta(uuid, "c3f71a0e-6d25-4b8e-9a14-2e8f5b7d0c63")

  struct ins
  {
    FloatMapInput<"XYZ map"> xyz;
    halp::file_path<"Calibration file", "Calibrations (*.json *.xyz.pfm)"> file;
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
    //! World from viewer camera and OpenGL projection, rows, for Camera's Matrix mode.
    halp::val_port<"Pose matrix", std::string> pose_rows;
    halp::val_port<"Projection matrix", std::string> projection_rows;
    halp::val_port<"Render size", halp::xy_type<int>> size;
    halp::val_port<"Planar", bool> planar;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
  } outputs;

  struct Settings
  {
    std::array<float, 3> viewer{};
    float planeTolerance{0.02f}, oversampling{1.f}, nearPlane{0.05f}, farPlane{1000.f};
    bool operator==(const Settings&) const = default;
  };

  struct Result
  {
    bool ok{};
    std::string status;
    std::array<float, 3> pa{}, pb{}, pc{};
    std::array<float, 4> frustum{};
    std::array<float, 16> view{}, projection{};
    std::string poseRows, projectionRows;
    int width{}, height{};
    bool planar{};
  };

  //! The frustum of a map, with the projection in OpenGL's convention.
  static Result solve(const std::vector<float>& xyz, int w, int h, const Settings& s);

  //! A calibration read again only when its listed XYZ map changes.
  struct Cache
  {
    std::mutex mutex;
    QString path, stamp;
    std::shared_ptr<const CalibrationFile> calibration;
    int loads{};
    std::shared_ptr<const CalibrationFile> get(const QString& path);
  };

  struct Job
  {
    QString file;
    std::shared_ptr<const std::vector<float>> xyz;
    int width{}, height{};
    Settings settings;
  };
  using Jobs = CalibrationJobs<Result>;
  struct State
  {
    Jobs jobs;
    Cache cache;
  };

  struct worker
  {
    std::function<void(std::weak_ptr<State>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(
        const std::weak_ptr<State>& s, uint64_t, const std::shared_ptr<const Job>&)
    {
      return !s.expired();
    }
    static void work(std::weak_ptr<State> s, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  //! Frames a Compute waits for the XYZ map input.
  static constexpr int readbackFrames = 30;
  //! Frames between two checks of the calibration file while Continuous.
  static constexpr int recheckFrames = 60;

  void operator()();

  const Cache& cache() const noexcept { return m_state->cache; }

private:
  Settings settings() const;
  void request(Job job);
  std::shared_ptr<State> m_state = std::make_shared<State>();
  uint64_t m_seen{};
  int m_wait{-1};
  int m_sinceCheck{};
  std::optional<Settings> m_requested;
  QString m_requestedFile;
};
}
