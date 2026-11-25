#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

// Gray-code structured light. The sequence: k = 0 white, k = 1 black, then the column
// bits MSB first, each followed by its inverse, then the row bits. Projector
// pixel (u, v), v down, is lit by a bit pattern when that bit of u ^ (u >> 1)
// is 1. Bits whose stripes are narrower than the minimum stripe width are not
// projected; their pixels decode to the centre of the coarser block.
// With phase shift, N patterns per axis follow the Gray bits (columns, then
// rows): I_k = level * (1/2 + 1/2 cos(2 pi u / P - 2 pi k / N)), pre-compensated
// for the projector gamma; the Gray code picks the period, the phase the
// position within it.
namespace Threedim::sl
{
struct GraySequence
{
  int width{1920};
  int height{1080};
  int bitsX{11};
  int bitsY{11};
  //! Low bits not projected, per axis.
  int skip{0};

  int phaseSteps{0};
  int period{0};
  float gamma{1.f};

  static GraySequence make(int width, int height, int minStripe) noexcept;
  //! Phase shift with `steps` patterns per axis. A period <= 0 picks the
  //! smallest power of two >= 4x the finest projected Gray stripe.
  void enablePhase(int steps, int period, float gamma) noexcept;
  static int autoPeriod(int skip) noexcept;

  int projectedX() const noexcept { return bitsX > skip ? bitsX - skip : 0; }
  int projectedY() const noexcept { return bitsY > skip ? bitsY - skip : 0; }
  int grayCount() const noexcept { return 2 + 2 * (projectedX() + projectedY()); }
  int count() const noexcept { return grayCount() + 2 * phaseSteps; }
  //! Width of the finest projected Gray stripe.
  int finestStripe() const noexcept { return 2 << skip; }

  bool lit(int k, int u, int v) const noexcept;
  //! The pattern value at pixel (u, v), 0..1, before level and gamma.
  float value(int k, int u, int v) const noexcept;
  bool isPhase(int k) const noexcept { return phaseSteps > 0 && k >= grayCount(); }
};

//! RGBA8, rows top to bottom.
void renderPattern(const GraySequence& seq, int k, float level, uint8_t* rgba);
//! A 64 px grid, the centre cross and a filled top-left cell.
void renderTestGrid(int width, int height, float level, uint8_t* rgba);

struct DecodedPixel
{
  float u{}, v{}, confidence{};
  bool valid{};
  //! u / v come from the unwrapped phase rather than the Gray block centre.
  bool phaseU{}, phaseV{};
  //! Within two camera pixels of an unlit one: biased by the blur.
  bool rim{};
  //! Clipped in a captured frame: its phase, and its light, are not measured.
  bool saturated{};
};

//! Accumulates the captured frames (linear luminance, one float per camera
//! pixel) of a sequence, in any order, and decodes them per camera pixel.
class Decoder
{
public:
  void reset(int width, int height, const GraySequence& seq);
  //! Frame k of the sequence; false, with error(), when k is not a frame of the
  //! sequence, was already added, or the frame is not of the decoder's size.
  bool add(int k, const std::vector<float>& lum);
  //! Camera pixels clipped in a frame, flagged saturated.
  void markSaturated(const std::vector<uint8_t>& clipped);
  //! The frames of the sequence not added yet.
  std::vector<int> missing() const;
  const std::string& error() const noexcept { return m_error; }

  //! A pixel needs white - black > minSignal and minBits bits per axis. A bit
  //! whose |I - Iinv| / (white - black) is under the contrast threshold sits on
  //! the edge of that bit's stripe: it is taken as it reads, the finer bits
  //! staying valid, and the second such bit ends the known bits. Every pixel
  //! is invalid while frames are missing.
  std::vector<DecodedPixel> decode(float minSignal, int minBits) const;
  //! Phase modulation B' / (white - black) under which the phase is not used.
  void setPhaseMinModulation(float m) noexcept { m_minModulation = m; }
  void setContrast(float contrast) noexcept { m_contrast = contrast; }

  int width() const noexcept { return m_w; }
  int height() const noexcept { return m_h; }
  //! The full white and full black frames (k = 0, 1).
  const std::vector<float>& white() const noexcept { return m_white; }
  const std::vector<float>& black() const noexcept { return m_black; }

private:
  struct Axis
  {
    //! The bit read for each projected bit, at its position in the code.
    std::vector<uint32_t> bits;
    //! The projected bits under the contrast threshold.
    std::vector<uint32_t> faint;
    std::vector<float> minContrast;
  };
  void addPair(int b, const std::vector<float>& I, const std::vector<float>& Iinv);
  void flush();

  int m_w{}, m_h{};
  GraySequence m_seq;
  std::vector<float> m_white, m_black;
  bool m_hasWhite{}, m_hasBlack{};
  std::vector<std::vector<float>> m_pending;
  std::vector<uint8_t> m_added;
  std::vector<uint8_t> m_saturated;
  Axis m_u, m_v;
  float m_contrast{0.15f};
  float m_minModulation{0.1f};
  std::vector<float> m_sinU, m_cosU, m_sinV, m_cosV;
  std::string m_error;
};

//! A sparse map of projector pixels: xyz, plus a weight (0 = empty).
struct SparseMap
{
  int width{}, height{};
  std::vector<std::array<float, 3>> value;
  std::vector<float> weight;

  void reset(int w, int h);
  //! Keeps the sample with the highest confidence.
  void keepBest(int u, int v, const std::array<float, 3>& x, float confidence);
  //! Sums the samples; read them as the average.
  void accumulate(int u, int v, const std::array<float, 3>& x);
  //! Bilinear splat at a sub-pixel position, weighted; read as the average.
  void splat(float u, float v, const std::array<float, 3>& x, float w);
};

//! Push-pull hole filling. Coarse levels average their children; when
//! `discontinuity` > 0 only the children within that distance of the block's
//! most central sample are averaged, so a hole next to a depth edge takes one
//! side rather than a point in mid-air. Only pixels within `holeRadius` of a
//! measured one are filled. Returns RGBA32F, rows top to bottom, w = 1 where
//! covered.
std::vector<float>
pushPullFill(const SparseMap& map, int holeRadius, float discontinuity, bool average);
}
