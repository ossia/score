#pragma once
#include <Threedim/StructuredLightSession.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{

// Projects the Gray-code sequence of CONVENTIONS.md, captures each pattern
// from a camera once it has settled, decodes on the CPU and outputs a
// calibration map per projector pixel:
//  - Depth camera: the measured world point (the camera's positions moved to
//    world), plus the projector intrinsics and pose solved from the 3D <-> 2D
//    correspondences;
//  - Viewer camera: the viewing direction from the camera (at the origin) to
//    the surface, the input of a cube / equirect lookup.
// The run is timed on the wall clock, so it works while the score plays live.
// Saving writes the map as PFM (name.xyz.pfm + name.valid.pfm) with a
// name.json sidecar; loading one re-outputs it.
class StructuredLightCalibration : public StructuredLightCore
{
public:
  halp_meta(name, "Structured Light Calibration")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "structured_light_calibration")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "Project a Gray-code sequence, capture it with a camera and output a per projector "
      "pixel XYZ map (depth camera) or viewing direction map (viewer camera), with the "
      "projector intrinsics and pose. Optional photometric phases after the Gray code: "
      "camera response recovery from frames at several exposures, sent on Camera "
      "exposure (Debevec & Malik 1997, doi:10.1145/258734.258884), which then linearises "
      "every capture instead of Camera gamma; the projector response per channel from a "
      "ramp of uniform levels (Majumder & Stevens 2004, doi:10.1109/TVCG.2004.1260769; "
      "Brown, Majumder & Yang 2005, doi:10.1109/TVCG.2005.27), made monotonic and "
      "interpolated with a monotone cubic (Fritsch & Carlson 1980, doi:10.1137/0717021), "
      "saved as name.response.json and a 1D name.response.cube; and the radiometric "
      "compensation of a coloured surface, a per pixel 3x3 colour mixing matrix from the "
      "red, green and blue primaries (Nayar, Peri, Grossberg & Belhumeur, PROCAMS 2003; "
      "C = V P + F as in Grossberg et al. 2004, doi:10.1109/CVPR.2004.1315067), "
      "inverted towards a global target c' = s c that a given fraction of the pixels "
      "can reach (the uniform initial scale of Grundhofer & Iwai 2015, "
      "doi:10.1109/TIP.2015.2478388), saved as name.comp.json for the Compensation of a "
      "multi-window output. "
      "Canvas places every projector's pattern in one image for a multi-window device.")
  halp_meta(uuid, "3f8c2a61-7d4e-4b19-9a05-c6e1d2b8f47a")


  struct ins
  {
    struct : halp::texture_input<"Camera", halp::rgba32f_texture>
    {
      bool request_readback{};
    } camera;
    struct : halp::texture_input<"Positions", halp::rgba32f_texture>
    {
      bool request_readback{};
    } positions;
    halp::val_port<"World from camera", std::array<float, 16>> world_from_camera{
        .value = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1}};
    struct ProjectorSize
        : halp::xy_spinboxes_i32<"Projector size", halp::range{1, 16384, 1920}>
    {
      ProjectorSize() { value = {1920, 1080}; }
    } projector_size;
    struct : halp::enum_t<Mode, "Mode">
    {
      struct range
      {
        std::string_view values[2]{
            "Depth camera -> XYZ map", "Viewer camera -> direction map"};
        Mode init{DepthCamera};
      };
    } mode;
    halp::hslider_f32<"Camera FOV", halp::range{5., 175., 60.}> fov;
    halp::xyzw_spinboxes_f32<"Camera intrinsics", halp::range{-100000., 100000., 0.}>
        intrinsics;
    halp::hslider_f32<"Pattern level", halp::range{0., 1., 1.}> level;
    halp::spinbox_i32<"Min stripe px", halp::irange{1, 256, 2}> min_stripe;
    halp::spinbox_i32<"Settle (ms)", halp::irange{0, 10000, 400}> settle;
    halp::spinbox_i32<"Frames to average", halp::irange{1, 32, 2}> frames;
    halp::hslider_f32<"Contrast threshold", halp::range{0.01, 1., 0.15}> contrast;
    halp::hslider_f32<"Min signal", halp::range{0., 1., 0.05}> min_signal;
    halp::spinbox_i32<"Min bits", halp::irange{1, 16, 6}> min_bits;
    halp::spinbox_i32<"Hole radius", halp::irange{0, 1024, 24}> hole_radius;
    halp::hslider_f32<"Depth jump (m)", halp::range{0., 2., 0.05}> depth_jump;
    //! 0: 3 px when the phase is valid on enough pixels, 12 px otherwise.
    halp::hslider_f32<"RANSAC threshold (px)", halp::range{0., 100., 0.}> ransac;
    struct : halp::enum_t<IdlePattern, "Idle pattern">
    {
      struct range
      {
        std::string_view values[3]{"Black", "Test grid", "White"};
        IdlePattern init{Black};
      };
    } idle;
    halp::impulse_button<"Start"> start;
    halp::impulse_button<"Abort"> abort;
    halp::save_file_path<"Save file"> save_path;
    halp::impulse_button<"Save"> save;
    struct load_t : halp::file_port<"Load file", halp::mmap_file_view>
    {
      halp_meta(extensions, "Calibration maps (*.pfm *.json)");
      static std::function<void(StructuredLightCalibration&)> process(file_type data);
    } load;
    struct : halp::enum_t<Surface, "Surface">
    {
      struct range
      {
        std::string_view values[3]{"Planes", "Smooth measured", "Raw measured"};
        Surface init{Planes};
      };
    } surface;
    halp::spinbox_i32<"Max planes", halp::irange{1, 8, 2}> max_planes;
    halp::hslider_f32<"Plane threshold (m)", halp::range{0.001, 0.5, 0.02}> plane_threshold;
    halp::toggle<"Phase shift", halp::toggle_setup{.init = true}> phase_shift;
    halp::spinbox_i32<"Phase steps", halp::irange{3, 16, 4}> phase_steps;
    halp::spinbox_i32<"Phase period px", halp::irange{0, 4096, 0}> phase_period;
    halp::hslider_f32<"Projector gamma", halp::range{0.5, 4., 2.2}> projector_gamma;
    halp::hslider_f32<"Phase min modulation", halp::range{0., 1., 0.1}> phase_min_modulation;
    struct : halp::enum_t<IntrinsicsMode, "Intrinsics mode">
    {
      struct range
      {
        std::string_view values[3]{"Solve", "Lock", "Lock to loaded"};
        IntrinsicsMode init{SolveIntrinsics};
      };
    } intrinsics_mode;
    //! fx, fy, cx, cy; 0 = unset.
    halp::xyzw_spinboxes_f32<"Locked intrinsics", halp::range{-100000., 100000., 0.}>
        locked_intrinsics;
    //! fx = fy = throw ratio x width, when Locked intrinsics is unset; 0 = unused.
    halp::hslider_f32<"Throw ratio", halp::range{0., 10., 0.}> throw_ratio;
    //! Fraction of the image, +x right, +y up (CONVENTIONS).
    halp::xy_spinboxes_f32<"Lens shift", halp::range{-4., 4., 0.}> lens_shift;
    halp::toggle<"Accumulate intrinsics"> accumulate_intrinsics;
    //! Camera values are raised to this power before the photometric maps.
    halp::hslider_f32<"Camera gamma", halp::range{0.2, 4., 1.}> camera_gamma;
    //! Projectors calibrated one after the other in one run, the others black:
    //! the first four on their own pattern outlet (Pattern, Pattern 2, ...),
    //! every one in its Canvas rect.
    halp::spinbox_i32<"Projector count", halp::irange{1, 16, 1}> projector_count;
    //! Uniform levels per channel (red, green, blue, grey) after the Gray
    //! code, for the projector response; 0 = off.
    halp::spinbox_i32<"Response levels", halp::irange{0, 64, 0}> response_levels;
    //! Full red, green and blue frames for the surface compensation.
    halp::toggle<"Surface compensation"> compensation;
    //! Values sent on Camera exposure, proportional to the exposure time,
    //! separated by spaces; empty = no camera response recovery.
    halp::lineedit<"Exposures", ""> exposures;
    halp::spinbox_i32<"Exposure settle (ms)", halp::irange{0, 20000, 1500}> exposure_settle;
    //! Sent on Camera exposure at the start and at the end of a run; 0 = never.
    halp::spinbox_f32<"Capture exposure", halp::range{0., 1000000., 0.}> capture_exposure;
    //! A name.crf.json to linearise the captures with when the run recovers none.
    halp::lineedit<"Camera response file", ""> crf_file;
    //! Fraction of the projector pixels that can show the compensated white.
    halp::hslider_f32<"Compensation in gamut", halp::range{0.5, 1., 0.85}> comp_gamut;
    halp::hslider_f32<"Compensation strength", halp::range{0., 1., 1.}> comp_strength;
    //! Also flatten the black (ambient and black level) to its maximum.
    halp::toggle<"Compensate black"> comp_black;
    //! "x y w h" per projector, normalised, separated by ';'; empty = side by side.
    halp::lineedit<"Canvas layout", ""> canvas_layout;
    //! 0 x 0 = no canvas.
    struct CanvasSize : halp::xy_spinboxes_i32<"Canvas size", halp::range{0, 16384, 0}>
    {
      CanvasSize() { value = {0, 0}; }
    } canvas_size;
    //! Save to Save file when a run is done.
    halp::toggle<"Auto save"> auto_save;
    //! Bound on the log-slope of the compensated white per projector pixel; 0, and with
    //! several projectors, = one white everywhere.
    halp::hslider_f32<"Compensation gradient", halp::range{0., 0.05, 0.}> comp_gradient;
    //! Uniform colours per axis measuring the projector's own colour mixing,
    //! with Surface compensation and Response levels; 0 = assume additive.
    halp::spinbox_i32<"Colour mixing levels", halp::irange{0, 9, 5}> mix_levels;
  } inputs;

  struct
  {
    halp::texture_output<"Pattern"> pattern;
    halp::texture_output<"XYZ map", halp::rgba32f_texture> xyz;
    halp::texture_output<"Decoded"> decoded;
    halp::val_port<"Intrinsics", std::array<float, 4>> intrinsics;
    halp::val_port<"Pose m0", std::array<float, 4>> m0;
    halp::val_port<"Pose m1", std::array<float, 4>> m1;
    halp::val_port<"Pose m2", std::array<float, 4>> m2;
    halp::val_port<"Pose m3", std::array<float, 4>> m3;
    halp::val_port<"World from projector", std::array<float, 16>> pose;
    halp::val_port<"RMS", float> rms;
    halp::val_port<"Coverage", float> coverage;
    halp::val_port<"Progress", float> progress;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
    struct
    {
      halp_meta(name, "Planes");
      std::vector<std::array<float, 4>> value;
    } planes;
    //! Median of the last 8 solved intrinsics of this session.
    halp::val_port<"Intrinsics (accumulated)", std::array<float, 4>> accumulated_intrinsics;
    //! Per projector pixel, linear camera RGB: white - black, and black.
    halp::texture_output<"Luminance map", halp::rgba32f_texture> lum;
    halp::texture_output<"Black map", halp::rgba32f_texture> black;
    halp::texture_output<"Pattern 2"> pattern2;
    halp::texture_output<"Pattern 3"> pattern3;
    halp::texture_output<"Pattern 4"> pattern4;
    //! The projector the patterns are on, 0 based.
    halp::val_port<"Projector index", int> projector_index;
    halp::texture_output<"XYZ map 2", halp::rgba32f_texture> xyz2;
    halp::texture_output<"XYZ map 3", halp::rgba32f_texture> xyz3;
    halp::texture_output<"XYZ map 4", halp::rgba32f_texture> xyz4;
    halp::val_port<"Camera exposure", std::optional<float>> camera_exposure;
    halp::val_port<"Auto exposure", std::optional<bool>> auto_exposure;
    //! Every projector's pattern in its Canvas layout rect.
    halp::texture_output<"Canvas"> canvas;
    //! Best single power per channel of the measured projector response.
    halp::val_port<"Measured gamma", std::array<float, 3>> measured_gamma;
  } outputs;

  //! The process instance: one object per render list exists for it (one per
  //! output device), and only one of them may drive the projector and camera.
  struct setup
  {
    int64_t instance{};
  };
  void prepare(setup s);
  struct Shared;
  StructuredLightCalibration() = default;
  StructuredLightCalibration(const StructuredLightCalibration&) = delete;
  StructuredLightCalibration& operator=(const StructuredLightCalibration&) = delete;
  ~StructuredLightCalibration();

  void operator()();
  //! One step of the state machine at wall-clock time nowMs.
  void tick(double nowMs);
  bool running() const noexcept { return m_session.running(); }
  const Result* lastResult() const noexcept
  {
    const auto& r = m_session.results();
    return r.empty() ? nullptr : r[0].get();
  }

private:
  void mirror();
  void publish(const Result& r);
  void syncPorts();
  void updateCanvas();
  void writeEvents(const StructuredLightSession::Events& ev);
  StructuredLightSession::Settings settings() const;

  std::shared_ptr<Shared> m_shared;
  int m_seenResult{};
  int m_published{};
  StructuredLightSession m_session;
  std::shared_ptr<Result> m_loaded;

  struct PortState
  {
    int kind{-1};
    int version{};
    int width{}, height{};
  };
  PortState m_ports[4];
  int m_canvasVersion{-1};
  int m_canvasW{}, m_canvasH{}, m_canvasCount{};
  std::string m_canvasLayout;
  int m_seenExposure{}, m_seenAuto{};
};
}
