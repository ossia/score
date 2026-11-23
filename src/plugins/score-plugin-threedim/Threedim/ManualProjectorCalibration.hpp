#pragma once
#include <Threedim/CalibrationJobs.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <QByteArray>
#include <QString>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
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
      "Reality (A K Peters). The pattern shows the crosshair, each recorded point and its "
      "reprojection. The recorded pairs are kept while the process runs and given on "
      "Recorded points, in the syntax of Points, to keep in the score; Save writes them "
      "with the calibration into name.json, as Structured Light Calibration does, with "
      "an XYZ map when surface planes are given.")
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
    bool operator==(const Pair&) const = default;
  };

  struct Settings
  {
    int width{1920}, height{1080};
    int intrinsicsMode{SolveIntrinsics};
    std::array<float, 4> lockedK{};
    float throwRatio{};
    std::array<float, 2> lensShift{};
    bool operator==(const Settings&) const = default;
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

  using Plane = std::array<float, 4>;

  struct Solved
  {
    Solution solution;
    int width{}, height{};
    //! Empty without surface planes.
    std::vector<float> xyz;
  };
  struct Loaded
  {
    bool ok{};
    std::string status;
    std::vector<Pair> pairs;
    int width{}, height{};
    Solution solution;
  };
  struct Saved
  {
    bool ok{};
    std::string status;
  };
  struct Pattern
  {
    int width{}, height{};
    std::vector<uint8_t> rgba;
  };

  struct SolveJob
  {
    std::vector<Pair> pairs;
    Settings settings;
    std::vector<Plane> planes;
  };
  struct SaveJob
  {
    std::string path;
    std::vector<Pair> pairs;
    Solution solution;
    Settings settings;
    std::vector<Plane> planes;
  };
  struct LoadJob
  {
    QString filename;
    QByteArray bytes;
  };
  struct PatternJob
  {
    int width{}, height{};
    std::array<float, 2> crosshair{};
    int size{};
    float level{};
    std::vector<Pair> pairs;
    Solution solution;
  };
  using Job = std::variant<SolveJob, SaveJob, LoadJob, PatternJob>;

  //! What the objects of every renderer of the node share: the recorded pairs
  //! and the jobs of the one that acts for the node.
  struct Shared
  {
    std::vector<Pair> recorded;
    uint64_t recordedVersion{1};
    CalibrationJobs<Solved> solves;
    CalibrationJobs<Loaded> loads;
    CalibrationJobs<Saved> saves;
    CalibrationJobs<Pattern> patterns;
  };

  struct ins
  {
    struct ProjectorSize
        : halp::xy_spinboxes_i32<"Projector size", halp::range{1, 8192, 1920}>
    {
      ProjectorSize() { value = {1920, 1080}; }
    } projector_size;
    //! Normalised, (0, 0) bottom-left as score's XY pad: y goes up.
    struct Crosshair : halp::xy_pad_f32<"Crosshair", halp::range{0., 1., 0.5}>
    {
      Crosshair() { value = {0.5f, 0.5f}; }
    } crosshair;
    halp::xyz_spinboxes_f32<"World point", halp::range{-10000., 10000., 0.}> world;
    halp::impulse_button<"Record"> record;
    halp::impulse_button<"Delete last"> delete_last;
    halp::impulse_button<"Clear"> clear;
    //! "u v X Y Z" per pair, pixels and metres, separated by ';' or new lines.
    struct : halp::lineedit<"Points", "">
    {
      void update(ManualProjectorCalibration& self) { self.m_typedDirty = true; }
    } points;
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
    struct : halp::lineedit<"Surface planes", "">
    {
      void update(ManualProjectorCalibration& self) { self.m_planesDirty = true; }
    } planes;
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
    //! The recorded pairs, as Points takes them.
    halp::val_port<"Recorded points", std::string> recorded;
  } outputs;

  struct worker
  {
    std::function<void(std::weak_ptr<Shared>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(
        const std::weak_ptr<Shared>& shared, uint64_t, const std::shared_ptr<const Job>&)
    {
      return !shared.expired();
    }
    static void work(std::weak_ptr<Shared> shared, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  std::shared_ptr<Shared> node_state;

  //! The pairs of `text`; `error` names the entries that are not 5 finite numbers.
  static std::vector<Pair> parsePairs(std::string_view text, std::string* error = nullptr);
  static std::vector<Plane> parsePlanes(std::string_view text, std::string* error = nullptr);
  static std::string formatPairs(const std::vector<Pair>& pairs);
  static Solution solve(const std::vector<Pair>& pairs, const Settings& s);
  //! RGBA per pixel, w = valid: the nearest plane in front of each pixel's ray.
  static std::vector<float>
  xyzFromPlanes(const Solution& sol, int w, int h, const std::vector<Plane>& planes);
  //! Merges into name.json (writeCalibration); a calibration without planes no
  //! longer lists an XYZ map, one without a solution no longer has a pose.
  static Saved saveFile(
      const std::string& path, const std::vector<Pair>& pairs, const Solution& sol,
      const Settings& s, const std::vector<Plane>& planes);
  static Loaded loadFile(const QString& filename, const QByteArray& bytes);
  //! RGBA8: crosshair, recorded points (green) and their reprojection (red).
  static void renderPattern(
      int w, int h, std::array<float, 2> crosshair, int size, float level,
      const std::vector<Pair>& pairs, const Solution& sol, uint8_t* rgba);

  ManualProjectorCalibration() = default;
  ManualProjectorCalibration(const ManualProjectorCalibration&) = delete;
  ManualProjectorCalibration& operator=(const ManualProjectorCalibration&) = delete;
  ~ManualProjectorCalibration();

  void operator()();

  //! The projector pixel (pixel centres at integers, v down) of a crosshair.
  static std::array<float, 2> crosshairPixel(std::array<float, 2> xy, int w, int h) noexcept
  {
    return {xy[0] * w - 0.5f, (1.f - xy[1]) * h - 0.5f};
  }

  const std::vector<Pair>& pairs() const noexcept { return m_all; }
  const Solution& solution() const noexcept { return m_solution; }

  bool m_typedDirty{true};
  bool m_planesDirty{true};

private:
  Shared& shared();
  Settings settings() const;
  void request(Job job, uint64_t id);
  void act(const Settings& s);
  void publish(const Solved& r);

  std::vector<Pair> m_typed, m_all;
  std::vector<Plane> m_planes;
  std::string m_inputError;
  Solution m_solution;
  uint64_t m_recordedSeen{}, m_solvedSeen{}, m_loadedSeen{}, m_savedSeen{}, m_patternSeen{};
  bool m_solveWanted{true};
  bool m_forceSolve{};
  Settings m_solvedSettings;
  std::array<float, 2> m_shownCrosshair{-1.f, -1.f};
  int m_shownSize{};
  float m_shownLevel{-1.f};
  bool m_patternWanted{true};
};
}
