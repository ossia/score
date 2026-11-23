#pragma once
#include <Gfx/Calibration/ProjectorModel.hpp>
#include <Threedim/CalibrationJobs.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace Threedim
{

class LedDiscovery
{
public:
  halp_meta(name, "LED Discovery")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "led_discovery")
  halp_meta(authors, "ossia team")
  halp_meta(
      description,
      "Find where each LED of a fixture is, with a camera: every LED blinks its index in "
      "binary, each bit frame followed by its inverse (2 + 2 ceil(log2(N + 1)) frames for "
      "N LEDs, against N for one-at-a-time scanning as MariMapper does, "
      "github.com/TheMariday/marimapper; the binary mode of Lightwork, "
      "github.com/PWRFLcreative/Lightwork; US 9,942,970 B2). Each camera pixel decodes "
      "an index; an LED's position is the brightness-weighted centroid of its pixels, "
      "and its world point the mean of a depth camera's positions there. Drive the LEDs "
      "with LED values (one per LED) or LED colours (r, g, b 0..255 per LED, for Art-Net "
      "/ sACN), sent when they change, or the Pattern texture (N x 1, in rows of 4096 above "
      "4096 LEDs). Positions are x, y, z, w per LED, sent once a discovery ends; w = 0 for "
      "an LED not found.")
  halp_meta(uuid, "f2a6c9e4-3b18-47d5-8e61-0c4b7d93a2f5")

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
        .value = score::gfx::procam::openCvToWorld};
    halp::spinbox_i32<"LED count", halp::irange{1, 1 << 20, 100}> count;
    halp::hslider_f32<"Level", halp::range{0., 1., 1.}> level;
    halp::spinbox_i32<"Settle (ms)", halp::irange{0, 10000, 300}> settle;
    halp::spinbox_i32<"Frames to average", halp::irange{1, 32, 2}> frames;
    halp::hslider_f32<"Min signal", halp::range{0., 1., 0.05}> min_signal;
    halp::hslider_f32<"Min contrast", halp::range{0., 1., 0.25}> min_contrast;
    halp::spinbox_i32<"Min pixels", halp::irange{1, 10000, 1}> min_pixels;
    halp::impulse_button<"Start"> start;
    halp::impulse_button<"Abort"> abort;
    halp::save_file_path<"CSV file"> csv;
    halp::impulse_button<"Save CSV"> save;
  } inputs;

  struct
  {
    halp::val_port<"LED values", std::optional<std::vector<float>>> values;
    halp::val_port<"LED colours", std::optional<std::vector<float>>> colours;
    halp::texture_output<"Pattern"> pattern;
    //! Normalized camera coordinates (v = 0 at the top), z = 0.
    halp::val_port<"Positions (camera)", std::optional<std::vector<float>>> uv;
    halp::val_port<"Positions (world)", std::optional<std::vector<float>>> world;
    halp::val_port<"Found", int> found;
    halp::val_port<"Progress", float> progress;
    halp::val_port<"Status", std::string> status;
    halp::val_port<"Done", std::optional<halp::impulse>> done;
  } outputs;

  struct Led
  {
    int index{};
    float u{}, v{}, nu{}, nv{};
    float x{}, y{}, z{};
    bool found{}, hasWorld{};
    int pixels{};
    float confidence{};
  };

  struct DecodeJob
  {
    int count{};
    std::vector<std::vector<float>> captures;
    int width{}, height{};
    //! Mean camera positions, x y z n per pixel; empty without a depth camera.
    std::vector<float> positions;
    std::array<float, 16> worldFromCamera{};
    float minSignal{}, minContrast{};
    int minPixels{};
  };
  struct Decoded
  {
    std::vector<Led> leds;
    std::vector<float> uv, world;
    int found{};
    std::string status;
  };
  struct CsvJob
  {
    std::string path;
    std::vector<Led> leds;
  };
  struct Written
  {
    std::string status;
  };
  static Decoded decode(const DecodeJob& job);

  //! The discovery, run by the object that acts for the node; the others show
  //! what it shows.
  struct Shared
  {
    CalibrationJobs<Decoded> decodes;
    CalibrationJobs<Written> writes;
    int frame{-1};
    bool decoding{};
    double shownAt{};
    double lastAt{-1e9};
    bool settled{};
    int taken{};
    int camW{}, camH{};
    std::vector<float> sum;
    std::vector<std::vector<float>> captures;
    std::vector<float> positions;
    int positionFrames{};
    std::vector<Led> leds;
    std::vector<float> values, colours;
    std::vector<uint8_t> pattern;
    int patternW{}, patternH{};
    uint64_t shown{}, results{}, statuses{};
    std::string status;
    float progress{};
    Decoded result;
  };

  struct Job
  {
    std::variant<DecodeJob, CsvJob> job;
  };
  struct worker
  {
    std::function<void(std::weak_ptr<Shared>, uint64_t, std::shared_ptr<const Job>)> request;
    static bool must_run(
        const std::weak_ptr<Shared>& s, uint64_t, const std::shared_ptr<const Job>&)
    {
      return !s.expired();
    }
    static void work(std::weak_ptr<Shared> s, uint64_t id, std::shared_ptr<const Job> job);
  } worker;

  std::shared_ptr<Shared> node_state;

  const std::vector<Led>& leds() noexcept { return state().leds; }
  //! The LED values and colours shown now.
  const std::vector<float>& values() noexcept { return state().values; }
  const std::vector<float>& colours() noexcept { return state().colours; }
  static constexpr int patternWidth = 4096;

  bool running() noexcept { return state().frame >= 0 || state().decoding; }
  void operator()();
  void tick(double nowMs);
  static bool writeCsv(const std::string& path, const std::vector<Led>& leds);

  LedDiscovery() = default;
  LedDiscovery(const LedDiscovery&) = delete;
  LedDiscovery& operator=(const LedDiscovery&) = delete;
  ~LedDiscovery();

private:
  Shared& state();
  void drive(double now);
  void mirror();
  void setStatus(std::string s);
  void show(int frame);
  void stop(const std::string& status);
  void finish();

  uint64_t m_shownSeen{}, m_resultSeen{}, m_statusSeen{}, m_decodedSeen{}, m_writtenSeen{};
};
}
