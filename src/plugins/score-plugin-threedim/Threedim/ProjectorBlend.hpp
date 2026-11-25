#pragma once
#include <Threedim/CalibrationFiles.hpp>
#include <Threedim/CalibrationJobs.hpp>
#include <Threedim/FloatTextureIO.hpp>

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

class ProjectorBlendMaps
{
public:
  halp_meta(name, "Projector Blend Maps")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "projector_blend_maps")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "Blend (alpha) and black-level (beta) maps, in linear light, for N projectors "
      "calibrated in one world frame. Each projector pixel is weighted by its distance to "
      "the edges of its lit area that another projector continues, normalised over the "
      "projectors that light the same point, found by reprojection with their pose or "
      "by inverting their XYZ map, with an occlusion test (Raskar et al. 1999, "
      "doi:10.1109/VISUAL.1999.809883; Brown, Majumder & Yang 2005, "
      "doi:10.1109/TVCG.2005.27). Optional luminance uniformity: the white is the "
      "largest one under the achievable white whose gradient stays under the "
      "perceptual bound |dW|/W <= 1/lambda (Majumder & Stevens 2005, "
      "doi:10.1145/1037957.1037964; a zero bound is the flat LAM of Majumder & Stevens "
      "2002, doi:10.1145/585740.585765). The black of the non-overlapped areas is "
      "raised to the overlap black (beta), and per projector RGB gains match the white "
      "points (Majumder et al. 2000, doi:10.1109/VISUAL.2000.885684). Per channel, "
      "with RGB luminance maps: each channel's white is bounded separately (Majumder & "
      "Stevens 2004, doi:10.1109/TVCG.2004.1260769) at the common white chromaticity, "
      "so alpha and beta are RGB and correct the colour of every pixel. Writes "
      "base.alpha.pfm and base.beta.pfm (grey, or RGB per channel) next to each "
      "calibration with base.blend.json, for the AlphaMap and BetaMap of a multi-window "
      "output with Linear blend on; the maps of the XYZ map inputs are written under "
      "Texture output name, name_p1, name_p2...")
  halp_meta(uuid, "a0d6f1c8-2b47-4e93-8f15-7c3e9b2d6a41")

  enum Reprojection
  {
    PoseWhenKnown,
    InvertMap
  };

  struct ins
  {
    struct : halp::lineedit<"Calibration files", "">
    {
      static constexpr bool path_list() { return true; }
    } files;
    FloatMapInput<"XYZ map 1"> xyz1;
    FloatMapInput<"XYZ map 2"> xyz2;
    FloatMapInput<"XYZ map 3"> xyz3;
    FloatMapInput<"XYZ map 4"> xyz4;
    halp::impulse_button<"Compute"> compute;
    //! Per projector, comma separated; one value applies to all.
    halp::lineedit<"Ramp widths (px)", "0"> ramp;
    halp::lineedit<"Blend exponents", "1"> exponent;
    //! Maps raised to 1 / gamma, for an output that does not blend in linear light.
    halp::hslider_f32<"Encode gamma", halp::range{0.5, 4., 1.}> encode_gamma;
    halp::toggle<"Black compensation", halp::toggle_setup{.init = true}> black_compensation;
    halp::hslider_f32<"Black level", halp::range{0., 0.2, 0.}> black_level;
    halp::toggle<"Photometric", halp::toggle_setup{.init = true}> photometric;
    halp::toggle<"Luminance uniformity"> uniformity;
    //! 900 tau / pi per radian, tau = 1% contrast threshold (Majumder & Stevens 2005, eq. 15).
    halp::hslider_f32<"Luminance gradient (1/rad)", halp::range{0., 20., 2.86}> gradient;
    halp::toggle<"Use viewer", halp::toggle_setup{.init = true}> use_viewer;
    struct Viewer : halp::xyz_spinboxes_f32<"Viewer", halp::range{-1000., 1000., 0.}>
    {
      Viewer() { value = {0.f, 1.6f, 0.f}; }
    } viewer;
    halp::hslider_f32<"Visibility tolerance (m)", halp::range{0., 1., 0.05}> tolerance;
    struct : halp::enum_t<Reprojection, "Reprojection">
    {
      struct range
      {
        std::string_view values[2]{"Pose when known", "Invert XYZ map"};
        Reprojection init{PoseWhenKnown};
      };
    } reprojection;
    //! The calibration set the texture inputs' maps are written as.
    halp::save_file_path<"Texture output name"> prefix;
    //! RGB alpha and beta when every calibration has an RGB luminance map.
    halp::toggle<"Per channel", halp::toggle_setup{.init = true}> per_channel;
  } inputs;

  struct
  {
    halp::texture_output<"Alpha 1", halp::rgba32f_texture> alpha1;
    halp::texture_output<"Alpha 2", halp::rgba32f_texture> alpha2;
    halp::texture_output<"Alpha 3", halp::rgba32f_texture> alpha3;
    halp::texture_output<"Alpha 4", halp::rgba32f_texture> alpha4;
    halp::texture_output<"Beta 1", halp::rgba32f_texture> beta1;
    halp::texture_output<"Beta 2", halp::rgba32f_texture> beta2;
    halp::texture_output<"Beta 3", halp::rgba32f_texture> beta3;
    halp::texture_output<"Beta 4", halp::rgba32f_texture> beta4;
    halp::val_port<"Alpha files", std::vector<std::string>> alpha_files;
    halp::val_port<"Beta files", std::vector<std::string>> beta_files;
    //! r, g, b linear gains per projector.
    halp::val_port<"Colour gains", std::vector<float>> gains;
    halp::val_port<"Overlap", std::vector<float>> overlap;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
  } outputs;

  struct Settings
  {
    std::vector<float> ramp, exponent;
    float encodeGamma{1.f};
    bool blackCompensation{true};
    float blackLevel{};
    bool photometric{true};
    bool uniformity{};
    float gradient{2.86f};
    bool useViewer{true};
    std::array<float, 3> viewer{0.f, 1.6f, 0.f};
    float tolerance{0.05f};
    bool invertMap{};
    bool perChannel{true};
  };

  struct Result
  {
    bool ok{};
    std::string status;
    std::vector<int> width, height;
    //! `channels` floats per pixel.
    std::vector<std::vector<float>> alpha, beta;
    int channels{1};
    std::vector<std::string> alphaFiles, betaFiles;
    std::vector<std::array<float, 3>> gains;
    std::vector<float> overlap;
    std::array<float, 3> whiteChroma{};
    float blackTarget{};
    std::vector<bool> reprojected;
    std::vector<float> rampWidth, exponent;
    double seconds{};
  };

  //! Blends the calibrations.
  static Result compute(std::vector<CalibrationFile> calibrations, const Settings& s);
  //! Writes the maps of `r` next to the calibrations that have a base, with
  //! their base.blend.json; the status says what was written.
  static void write(Result& r, const std::vector<QString>& bases, const Settings& s);

  struct Job
  {
    std::vector<std::string> paths;
    std::vector<CalibrationFile> textures;
    Settings settings;
  };
  //! Loads, blends and writes.
  static Result run(const Job& job);

  using Jobs = CalibrationJobs<Result>;
  struct worker
  {
    std::function<void(std::weak_ptr<Jobs>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(const std::weak_ptr<Jobs>& jobs, uint64_t, const std::shared_ptr<const Job>&)
    {
      return !jobs.expired();
    }
    static void work(std::weak_ptr<Jobs> jobs, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  std::shared_ptr<Jobs> node_state;

  //! Frames a Compute waits for the XYZ map inputs.
  static constexpr int readbackFrames = 3;

  ProjectorBlendMaps() = default;
  ProjectorBlendMaps(const ProjectorBlendMaps&) = delete;
  ProjectorBlendMaps& operator=(const ProjectorBlendMaps&) = delete;
  ~ProjectorBlendMaps();

  void operator()();

  //! A settings parse failure, or empty.
  static std::string parseSettings(const ins& in, Settings& s);

private:
  Jobs& jobs();
  void start();
  void publish(const Result& r);
  int m_waitFrames{-1};
  uint64_t m_seen{};
};
}
