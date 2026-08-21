#pragma once

// A frame-indexed test pattern generated here, in the test, and never by score.
//
// Nothing below reads a score header, calls a score function, or is derived from
// score's own idea of a pixel: the bytes are written by writeRawFrames() and
// handed to an external process (gst-launch, ffmpeg), so a picture that matches
// them came through the code under test rather than around it. A harness whose
// producer and consumer are built from one shared format triple only proves that
// it agrees with itself.
//
// Two properties from one pattern:
//
//   * exact content. Every block is one of eight colours whose channels are only
//     0 or 255 -- fixed points of every gamma curve, sRGB conversion and 8-bit
//     rounding step in a render pipeline -- so the comparison can demand equality
//     rather than a tolerance, and the interior of a flat block survives
//     resampling, which is what lets a 320x240 source be asserted out of a
//     1920x1080 readback.
//
//   * a self-identifying frame number. The top row of blocks spells the frame
//     index in base 8, so a grabbed picture says which frame it is. A frozen
//     texture, a repeated buffer and a stale readback all keep reporting the
//     same index.
//
// The other three rows are a function of the index too, so a picture cannot
// match while carrying content from a different frame: 1 in 8^12 by chance.

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QString>

#include <array>

namespace MovingPattern
{

inline constexpr int kWidth = 160;
inline constexpr int kHeight = 120;
inline constexpr int kCols = 4;
inline constexpr int kRows = 4;

//! Eight colours with 0/255 channels only: exactly representable everywhere,
//! and unchanged by any transfer function a render target might apply.
inline constexpr std::array<std::array<int, 3>, 8> kPalette{{
    {{0, 0, 0}},
    {{255, 0, 0}},
    {{0, 255, 0}},
    {{0, 0, 255}},
    {{255, 255, 0}},
    {{255, 0, 255}},
    {{0, 255, 255}},
    {{255, 255, 255}},
}};

//! Largest frame index the top row can spell: four base-8 digits.
inline constexpr int kMaxIndex = 8 * 8 * 8 * 8;

//! Palette slot of block (bx, by) on frame @p k. Row 0 is the base-8 spelling
//! of k, least significant digit on the left; the rest is content that changes
//! in every block on every frame (7 is coprime with 8).
inline int paletteIndex(int k, int bx, int by) noexcept
{
  if(by == 0)
  {
    int div = 1;
    for(int i = 0; i < bx; i++)
      div *= 8;
    return (k / div) % 8;
  }
  return ((k * 7) + bx * 3 + by * 5) % 8;
}

//! Frame index spelled by the top row of an already-classified picture.
inline int indexFromTopRow(const std::array<int, kCols>& digits) noexcept
{
  int k = 0, mul = 1;
  for(int bx = 0; bx < kCols; bx++)
  {
    k += digits[bx] * mul;
    mul *= 8;
  }
  return k;
}

//! Deliberate defects a run can inject into the *producer* to prove that the
//! assertions are load-bearing. Nothing about the assertions changes; only the
//! bytes that go on the wire do, and each one must turn a specific check red.
enum class Mutation
{
  None,
  //! Every frame is frame 0: a picture that is perfectly valid content and
  //! never moves. Turns the motion assertion red, leaves the content one green.
  Frozen,
  //! Correct frame numbering, content rows rotated by one palette slot: a
  //! picture that advances and is wrong. Turns the content assertion red.
  WrongContent,
};

inline Mutation mutationFromEnvironment()
{
  const QByteArray v = qgetenv("SCORE_TEST_PATTERN_NEGATIVE").toLower();
  if(v == "frozen")
    return Mutation::Frozen;
  if(v == "wrong")
    return Mutation::WrongContent;
  return Mutation::None;
}

//! @p n frames of tightly packed RGBA8, one after the other. This is the file
//! an external producer replays; score never sees this code.
inline bool writeRawFrames(const QString& path, int n, Mutation mut = Mutation::None)
{
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
    return false;

  const int blockW = kWidth / kCols;
  const int blockH = kHeight / kRows;

  QByteArray frame;
  frame.resize(kWidth * kHeight * 4);
  for(int frameNo = 0; frameNo < n; frameNo++)
  {
    const int k = (mut == Mutation::Frozen) ? 0 : frameNo;
    auto* p = reinterpret_cast<unsigned char*>(frame.data());
    for(int y = 0; y < kHeight; y++)
    {
      const int by = qMin(y / blockH, kRows - 1);
      for(int x = 0; x < kWidth; x++)
      {
        const int bx = qMin(x / blockW, kCols - 1);
        int slot = paletteIndex(k, bx, by);
        if(mut == Mutation::WrongContent && by > 0)
          slot = (slot + 1) % 8;
        const auto& c = kPalette[slot];
        *p++ = static_cast<unsigned char>(c[0]);
        *p++ = static_cast<unsigned char>(c[1]);
        *p++ = static_cast<unsigned char>(c[2]);
        *p++ = 255;
      }
    }
    if(f.write(frame) != frame.size())
      return false;
  }
  f.close();
  return true;
}

//! How a grabbed picture is laid out relative to the source. Which one holds is
//! a property of the render path, not of this pattern; the pattern is asymmetric
//! in both axes so a wrong guess cannot accidentally match.
enum class Orientation
{
  TopLeft,   //!< source row 0 is readback row 0
  BottomLeft //!< vertically flipped
};

struct Reading
{
  int frame{-1};       //!< index spelled by the top row, -1 if unreadable
  int sampled{0};      //!< pixels compared
  int mismatched{0};   //!< pixels that were not exactly the expected colour
  bool uniform{false}; //!< the whole picture is one colour: no frame arrived
  //! the picture is exactly this frame, vertically flipped. Still a failure,
  //! but a different one from "not this frame at all".
  bool flippedWouldMatch{false};
  Orientation orientation{Orientation::TopLeft};

  bool exact() const noexcept
  {
    return !uniform && frame >= 0 && sampled > 0 && mismatched == 0;
  }
};

//! True when every pixel is the same colour. The readback before the first
//! buffer arrives is a cleared target, and a single flat colour can never be a
//! frame of this pattern -- every frame has at least two distinct blocks.
inline bool isUniform(const QImage& img)
{
  if(img.isNull())
    return true;
  const QRgb first = img.pixel(0, 0);
  for(int y = 0; y < img.height(); y++)
    for(int x = 0; x < img.width(); x++)
      if((img.pixel(x, y) & 0x00FFFFFF) != (first & 0x00FFFFFF))
        return false;
  return true;
}

namespace detail
{
inline int nearestPaletteSlot(QRgb c) noexcept
{
  int best = -1;
  long long bestD = -1;
  for(int i = 0; i < 8; i++)
  {
    const long long dr = qRed(c) - kPalette[i][0];
    const long long dg = qGreen(c) - kPalette[i][1];
    const long long db = qBlue(c) - kPalette[i][2];
    const long long d = dr * dr + dg * dg + db * db;
    if(bestD < 0 || d < bestD)
    {
      bestD = d;
      best = i;
    }
  }
  return best;
}

//! Compare every pixel of the inner half of every block against the colour the
//! generator put there. The inner half is what makes the comparison exact under
//! an arbitrary rescale: a flat region's interior is invariant under nearest and
//! bilinear filtering alike, while its edges are not.
inline Reading readAt(const QImage& img, Orientation o)
{
  Reading r;
  r.orientation = o;
  if(img.isNull() || img.width() < kCols * 8 || img.height() < kRows * 8)
    return r;

  const double blockW = double(img.width()) / kCols;
  const double blockH = double(img.height()) / kRows;

  // The frame index first: read the top row's four blocks at their centres and
  // snap each to the nearest palette slot.
  std::array<int, kCols> digits{};
  for(int bx = 0; bx < kCols; bx++)
  {
    const int srcRow = 0;
    const int imgRow = (o == Orientation::TopLeft) ? srcRow : (kRows - 1 - srcRow);
    const int px = int((bx + 0.5) * blockW);
    const int py = int((imgRow + 0.5) * blockH);
    digits[bx] = nearestPaletteSlot(img.pixel(px, py));
  }
  const int k = indexFromTopRow(digits);
  if(k < 0 || k >= kMaxIndex)
    return r;
  r.frame = k;

  for(int by = 0; by < kRows; by++)
  {
    const int imgRow = (o == Orientation::TopLeft) ? by : (kRows - 1 - by);
    for(int bx = 0; bx < kCols; bx++)
    {
      const auto& c = kPalette[paletteIndex(k, bx, by)];
      const QRgb want = qRgb(c[0], c[1], c[2]);

      const int x0 = int((bx + 0.25) * blockW);
      const int x1 = int((bx + 0.75) * blockW);
      const int y0 = int((imgRow + 0.25) * blockH);
      const int y1 = int((imgRow + 0.75) * blockH);
      for(int y = y0; y < y1; y++)
        for(int x = x0; x < x1; x++)
        {
          r.sampled++;
          if((img.pixel(x, y) & 0x00FFFFFF) != (want & 0x00FFFFFF))
            r.mismatched++;
        }
    }
  }
  return r;
}
} // namespace detail

//! Read a grabbed picture. Top-left is the only orientation that counts as a
//! match -- an upside-down picture is a wrong picture, and this pattern is
//! asymmetric in both axes so it can say so. The flipped reading is taken only
//! to name the symptom: "it is the right frame, upside down" and "it is not the
//! frame at all" are different defects and should not report the same way.
inline Reading read(const QImage& in)
{
  QImage img = in.convertToFormat(QImage::Format_RGB32);
  if(isUniform(img))
  {
    Reading r;
    r.uniform = true;
    return r;
  }
  auto tl = detail::readAt(img, Orientation::TopLeft);
  if(!tl.exact() && detail::readAt(img, Orientation::BottomLeft).exact())
    tl.flippedWouldMatch = true;
  return tl;
}

inline Reading readFile(const QString& path)
{
  QImage img{path};
  if(img.isNull())
    return {};
  return read(img);
}

} // namespace MovingPattern
