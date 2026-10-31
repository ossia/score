#pragma once
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{

class ManualProjectorCalibration
{
public:
  halp_meta(name, "Manual Projector Calibration")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "manual_projector_calibration")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "Calibrate a projector without a camera: move the crosshair onto a known point of "
      "the venue (CAD vertex, measured mark), give its world position, record the pair. "
      "6+ pairs off a single plane solve the intrinsics and pose (normalised DLT, Hartley "
      "& Zisserman, Multiple View Geometry 2nd ed. 2004, sec. 4.4 and 7.1, then "
      "Levenberg-Marquardt on the reprojection error); with locked intrinsics 4+ pairs "
      "solve the pose (EPnP, Lepetit, Moreno-Noguer & Fua 2009, "
      "doi:10.1007/s11263-008-0152-6, or the plane homography for coplanar points). The "
      "interactive workflow follows Raskar et al. 2001, Shader Lamps "
      "(doi:10.2312/EGWR/EGWR01/089-101) and Bimber & Raskar 2005, Spatial Augmented "
      "Reality (A K Peters). The "
      "pattern shows the crosshair, each recorded point and its reprojection. Saves "
      "name.json as Structured Light Calibration does, with an XYZ map when surface planes "
      "are given.")
  halp_meta(uuid, "6b0e3f2a-91d4-4c58-a7e2-3d5c8f14b9a0")

  enum IntrinsicsMode
  {
    SolveIntrinsics,
    LockIntrinsics
  };

  struct Pair
  {
    float u{}, v{};
    std::array<float, 3> world{};
  };

  struct Settings
  {
    int width{1920}, height{1080};
    int intrinsicsMode{SolveIntrinsics};
    std::array<float, 4> lockedK{};
    float throwRatio{};
    std::array<float, 2> lensShift{};
  };

  struct Solution
  {
    bool ok{};
    std::string status;
    std::array<float, 4> intrinsics{};
    std::array<float, 16> pose{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float rms{};
    //! Reprojected pixel and error per pair, -1 when behind the projector.
    std::vector<std::array<float, 2>> reprojected;
    std::vector<float> errors;
    bool intrinsicsLocked{};
  };

  struct ins
  {
    struct ProjectorSize
        : halp::xy_spinboxes_i32<"Projector size", halp::range{1, 16384, 1920}>
    {
      ProjectorSize() { value = {1920, 1080}; }
    } projector_size;
    //! Normalised, (0, 0) top-left.
    struct Crosshair : halp::xy_pad_f32<"Crosshair", halp::range{0., 1., 0.5}>
    {
      Crosshair() { value = {0.5f, 0.5f}; }
    } crosshair;
    halp::xyz_spinboxes_f32<"World point", halp::range{-10000., 10000., 0.}> world;
    halp::impulse_button<"Record"> record;
    halp::impulse_button<"Delete last"> delete_last;
    halp::impulse_button<"Clear"> clear;
    //! "u v X Y Z" per pair, pixels and metres, separated by ';' or new lines.
    halp::lineedit<"Points", ""> points;
    struct : halp::enum_t<IntrinsicsMode, "Intrinsics mode">
    {
      struct range
      {
        std::string_view values[2]{"Solve", "Lock"};
        IntrinsicsMode init{SolveIntrinsics};
      };
    } intrinsics_mode;
    halp::xyzw_spinboxes_f32<"Locked intrinsics", halp::range{-100000., 100000., 0.}>
        locked_intrinsics;
    halp::hslider_f32<"Throw ratio", halp::range{0., 10., 0.}> throw_ratio;
    halp::xy_spinboxes_f32<"Lens shift", halp::range{-4., 4., 0.}> lens_shift;
    halp::toggle<"Auto solve", halp::toggle_setup{.init = true}> auto_solve;
    halp::impulse_button<"Solve"> solve;
    //! "nx ny nz d" per plane, n . p + d = 0, as Structured Light Calibration outputs them.
    halp::lineedit<"Surface planes", ""> planes;
    halp::spinbox_i32<"Crosshair size", halp::irange{4, 1024, 40}> crosshair_size;
    halp::hslider_f32<"Pattern level", halp::range{0., 1., 1.}> level;
    halp::save_file_path<"Save file"> save_path;
    halp::impulse_button<"Save"> save;
    struct load_t : halp::file_port<"Load file", halp::mmap_file_view>
    {
      halp_meta(extensions, "Calibrations (*.json)");
      static std::function<void(ManualProjectorCalibration&)> process(file_type data);
    } load;
  } inputs;

  struct
  {
    halp::texture_output<"Pattern"> pattern;
    halp::val_port<"Intrinsics", std::array<float, 4>> intrinsics;
    halp::val_port<"Pose m0", std::array<float, 4>> m0;
    halp::val_port<"Pose m1", std::array<float, 4>> m1;
    halp::val_port<"Pose m2", std::array<float, 4>> m2;
    halp::val_port<"Pose m3", std::array<float, 4>> m3;
    halp::val_port<"World from projector", std::array<float, 16>> pose;
    halp::val_port<"RMS", float> rms;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
    halp::val_port<"Point count", int> count;
    halp::val_port<"Errors", std::vector<float>> errors;
    halp::texture_output<"XYZ map", halp::rgba32f_texture> xyz;
  } outputs;

  static std::vector<Pair> parsePairs(std::string_view text);
  static std::vector<std::array<float, 4>> parsePlanes(std::string_view text);
  static Solution solve(const std::vector<Pair>& pairs, const Settings& s);
  //! RGBA per pixel, w = valid: the nearest plane in front of each pixel's ray.
  static std::vector<float> xyzFromPlanes(
      const Solution& sol, int w, int h, const std::vector<std::array<float, 4>>& planes);
  static bool saveFile(
      const std::string& path, const std::vector<Pair>& pairs, const Solution& sol,
      const Settings& s, const std::vector<std::array<float, 4>>& planes);
  struct Loaded
  {
    bool ok{};
    std::string status;
    std::vector<Pair> pairs;
    int width{}, height{};
    Solution solution;
  };
  static Loaded loadFile(const std::string& path);
  //! RGBA8: crosshair, recorded points (green) and their reprojection (red).
  static void renderPattern(
      int w, int h, std::array<float, 2> crosshair, int size, float level,
      const std::vector<Pair>& pairs, const Solution& sol, uint8_t* rgba);

  void operator()();

  const std::vector<Pair>& pairs() const noexcept { return m_all; }
  const Solution& solution() const noexcept { return m_solution; }

private:
  Settings settings() const;
  void rebuild();
  void runSolve();
  void publish();

  std::vector<Pair> m_recorded;
  std::vector<Pair> m_typed;
  std::vector<Pair> m_all;
  std::string m_typedText;
  Solution m_solution;
  std::shared_ptr<Loaded> m_loaded;
  bool m_dirty{true};
  bool m_patternDirty{true};
  std::array<float, 2> m_shownCrosshair{-1.f, -1.f};
  int m_shownW{}, m_shownH{}, m_shownSize{};
  float m_shownLevel{-1.f};
};
}
