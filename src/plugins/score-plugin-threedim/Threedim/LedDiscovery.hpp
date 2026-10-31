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
      "/ sACN), or the Pattern texture (N x 1). Positions are x, y, z, w per LED for LED "
      "Sampler; w = 0 for an LED not found.")
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
        .value = {1, 0, 0, 0, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1}};
    halp::spinbox_i32<"LED count", halp::irange{1, 1 << 20, 100}> count;
    halp::hslider_f32<"Level", halp::range{0., 1., 1.}> level;
    halp::spinbox_i32<"Settle (ms)", halp::irange{0, 10000, 300}> settle;
    halp::spinbox_i32<"Frames to average", halp::irange{1, 32, 2}> frames;
    halp::hslider_f32<"Min signal", halp::range{0., 1., 0.05}> min_signal;
    halp::hslider_f32<"Min contrast", halp::range{0., 1., 0.25}> min_contrast;
    halp::spinbox_i32<"Min pixels", halp::irange{1, 10000, 1}> min_pixels;
    halp::impulse_button<"Start"> start;
    halp::impulse_button<"Abort"> abort;
    halp::lineedit<"CSV file", ""> csv;
    halp::impulse_button<"Save CSV"> save;
  } inputs;

  struct
  {
    halp::val_port<"LED values", std::vector<float>> values;
    halp::val_port<"LED colours", std::vector<float>> colours;
    halp::texture_output<"Pattern"> pattern;
    //! Normalized camera coordinates (v = 0 at the top), z = 0.
    halp::val_port<"Positions (camera)", std::vector<float>> uv;
    halp::val_port<"Positions (world)", std::vector<float>> world;
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
  const std::vector<Led>& leds() const noexcept { return m_leds; }

  bool running() const noexcept { return m_frame >= 0; }
  void operator()();
  void tick(double nowMs);
  static bool writeCsv(const std::string& path, const std::vector<Led>& leds);

private:
  void show(int frame);
  void finish();

  int m_frame{-1};
  double m_shownAt{};
  double m_lastAt{-1e9};
  bool m_settled{};
  int m_taken{};
  int m_camW{}, m_camH{};
  std::vector<float> m_sum;
  std::vector<std::vector<float>> m_captures;
  std::vector<float> m_positions;
  int m_positionFrames{};
  std::vector<Led> m_leds;
};
}
