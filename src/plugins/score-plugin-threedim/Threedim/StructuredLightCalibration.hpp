#pragma once
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/FloatTextureIO.hpp>
#include <Threedim/StructuredLightSession.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Threedim
{

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
      "multi-window output. The run waits for each pattern to reach the camera, stops "
      "with a status when no frame or a frozen one comes, and keeps the camera's "
      "exposure manual until it ends. Pattern, Pattern 2... are each projector's "
      "image, for hand wiring; the Projector calibration tool runs the same session "
      "on the outputs of a multi-window device.")
  halp_meta(uuid, "3f8c2a61-7d4e-4b19-9a05-c6e1d2b8f47a")


  struct ins
  {
    struct : halp::texture_input<"Camera", halp::custom_variable_texture>
    {
      bool request_readback{};
      bool resampled{};
    } camera;
    FloatMapInput<"Positions"> positions;
    halp::val_port<"World from camera", std::array<float, 16>> world_from_camera{
        .value = score::gfx::procam::openCvToWorld};
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
    //! A pattern whose frame does not come, or does not change, within this fails.
    halp::spinbox_i32<"Timeout (ms)", halp::irange{100, 60000, 5000}> timeout;
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
    struct : halp::file_path<"Load file", "Calibrations (*.json *.xyz.pfm)">
    {
      void update(StructuredLightCalibration& self) { self.m_loadWanted = true; }
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
    //! Fraction of the image, +x right, +y up.
    halp::xy_spinboxes_f32<"Lens shift", halp::range{-4., 4., 0.}> lens_shift;
    halp::toggle<"Accumulate intrinsics"> accumulate_intrinsics;
    //! Camera values are raised to this power before the photometric maps.
    halp::hslider_f32<"Camera gamma", halp::range{0.2, 4., 1.}> camera_gamma;
    //! Projectors calibrated one after the other in one run, the others black,
    //! each on its pattern outlet (Pattern, Pattern 2, ...).
    halp::spinbox_i32<"Projector count", halp::irange{1, 4, 1}> projector_count;
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
    //! A name.crf.json to linearise the captures with.
    halp::file_path<"Camera response file", "Camera responses (*.crf.json)"> crf_file;
    //! Fraction of the projector pixels that can show the compensated white.
    halp::hslider_f32<"Compensation in gamut", halp::range{0.5, 1., 0.85}> comp_gamut;
    halp::hslider_f32<"Compensation strength", halp::range{0., 1., 1.}> comp_strength;
    //! Also flatten the black (ambient and black level) to its maximum.
    halp::toggle<"Compensate black"> comp_black;
    //! Save to Save file when a run is done.
    halp::toggle<"Auto save"> auto_save;
    //! Bound on the log-slope of the compensated white per projector pixel; 0, and with
    //! several projectors, = one white everywhere.
    halp::hslider_f32<"Compensation gradient", halp::range{0., 0.05, 0.}> comp_gradient;
    //! Uniform colours per axis measuring the projector's own colour mixing,
    //! with Surface compensation and Response levels; 0 = assume additive.
    halp::spinbox_i32<"Colour mixing levels", halp::irange{0, 9, 5}> mix_levels;
    //! Sent on Auto exposure when a run ends: what the camera had before it.
    halp::toggle<"Auto exposure after", halp::toggle_setup{.init = true}> auto_exposure_after;
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
    //! Best single power per channel of the measured projector response.
    halp::val_port<"Measured gamma", std::array<float, 3>> measured_gamma;
  } outputs;

  //! What the objects of the renderers of the node share: the session, run by
  //! the object whose renderer drives the node.
  struct Shared
  {
    StructuredLightSession session;
    const void* driver{};
  };
  std::shared_ptr<Shared> node_state;
  bool drives_node{true};

  struct worker
  {
    std::function<void(std::function<void()>)> request;
    static bool must_run(const std::function<void()>&) { return true; }
    static void work(std::function<void()> job) { job(); }
  } worker;

  StructuredLightCalibration() = default;
  StructuredLightCalibration(const StructuredLightCalibration&) = delete;
  StructuredLightCalibration& operator=(const StructuredLightCalibration&) = delete;
  ~StructuredLightCalibration();

  void operator()();
  //! One step at wall-clock time nowMs.
  void tick(double nowMs);
  bool running() noexcept { return shared().session.running(); }
  const Result* lastResult() noexcept
  {
    const auto& r = shared().session.results();
    return r.empty() ? nullptr : r[0].get();
  }

  bool m_loadWanted{};
  //! Keep the per camera pixel decode in the results.
  bool keepPixels{};

private:
  Shared& shared();
  void drive(Shared& sh, double now);
  void publish(const Result& r);
  void syncPorts(const StructuredLightSession& session);
  void writeEvents(const StructuredLightSession::Events& ev);
  StructuredLightSession::Settings settings() const;
  const float* cameraFrame();

  int m_published{-1};
  int m_versions[4]{-1, -1, -1, -1};
  std::vector<float> m_frame;
};
}
