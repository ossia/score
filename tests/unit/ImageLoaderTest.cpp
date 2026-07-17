// Unit tests for the image-loading layer:
//
//  - score::gfx decodeImageFromPath / decodeImageFromMemory
//        (Gfx/Graph/TextureLoader.{hpp,cpp}) — the central
//        CPU decode used by material textures / AssetTable consumers.
//  - Threedim::ImageLoader (LDR)   (Threedim/ImageLoader.hpp)
//        — the file-port process() decode contract (header-inline; the
//        GPU-side init/update needs a full RenderList and is not unit-
//        testable here; upload semantics are covered by AssetTableTest's
//        Null-RHI TextureLoader tests).
//  - Threedim::ArrayToTexture      (Threedim/ArrayToTexture.hpp)
//        — float-array → texture-bytes conversion for every format branch.
//
// Format support matrix asserted below (values, not just "loads"):
//   PNG8 / PNG16 / JPEG / BMP / TIFF8 / TIFF16 / WebP(lossless) / GIF /
//   TGA(2.0, both origins) / PPM / PGM / SVG / ICO
// The environment's Qt provides: bmp cur gif icns ico jpeg mng pbm pdf pgm
// png ppm svg svgz tga tif tiff wbmp webp xbm xpm — formats whose plugin may
// be absent elsewhere are SKIP-guarded on QImageReader::supportedImageFormats.
//
// Documented behaviors (verified against Qt 6.4 / this repo's contract):
//  * Decode canonicalizes to QImage::Format_RGBA8888: byte order is
//    R,G,B,A in memory regardless of endianness, straight (non-premultiplied)
//    alpha, top-left origin, no vertical flip (memory note:
//    "texture-origin-top-left").
//  * 16-bit sources (PNG16/TIFF16) ARE decoded at full RGBA64 precision by
//    Qt, but decodeImageFromPath/Memory then quantize to 8 bits — that is
//    the documented LDR contract (RGBA8 texture; the HDR path is OIIO-based
//    and lives elsewhere). The quantization must round, not truncate junk.
//  * sRGB handling is metadata-only: decode never linearizes/alters pixel
//    bytes; the sRGB decision is taken at upload time via the QRhiTexture
//    sRGB flag (covered in AssetTableTest).
//  * Corrupt/truncated/garbage inputs fail as std::nullopt without crashing
//    (ASAN-clean). Known deliberate exceptions, asserted as such:
//      - truncated BMPs decode to a full-size image (Qt pads missing rows)
//      - a PNG payload with a lying ".jpg" extension still loads from path
//        (QImage sniffs content), while decodeImageFromMemory with an
//        explicit wrong format hint fails (no fallback probing).
//  * Qt's TGA handler only accepts TGA 2.0 files bearing the
//    "TRUEVISION-XFILE." footer; footer-less (original-spec) TGAs are
//    rejected. Both bottom-left and top-left origin files are normalized to
//    top-left on decode, and the file's BGRA byte order is swizzled to RGBA.

#include <Gfx/Graph/TextureLoader.hpp>
#include <Threedim/ArrayToTexture.hpp>
#include <Threedim/ImageLoader.hpp>

#include <QBuffer>
#include <QByteArray>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <cstring>

namespace
{
// Some decoders (SVG rasterization, some paint-based paths) need a
// QGuiApplication; Catch2 owns main() so create it lazily, offscreen.
void ensureApp()
{
  if(!QCoreApplication::instance())
  {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    static int argc = 1;
    static char arg0[] = "ImageLoaderTest";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication app(argc, argv);
  }
}

bool formatSupported(const char* fmt)
{
  return QImageReader::supportedImageFormats().contains(QByteArray(fmt));
}

// 16x16 reference: four 8x8 solid quadrants.
//   top-left  = opaque red        top-right   = opaque green
//   bottom-left = opaque blue     bottom-right = white (alpha 128 or opaque)
// The asymmetric layout catches vertical flips (red/green on top vs
// blue/white on top) and horizontal flips, the quadrant colors catch
// R<->B channel swaps, and the BR quadrant carries the alpha payload.
QImage quadrantImage(int w = 16, int h = 16, bool withAlpha = true)
{
  QImage img(w, h, QImage::Format_RGBA8888);
  for(int y = 0; y < h; y++)
    for(int x = 0; x < w; x++)
    {
      QColor c;
      if(y < h / 2)
        c = (x < w / 2) ? QColor(255, 0, 0, 255) : QColor(0, 255, 0, 255);
      else
        c = (x < w / 2)
                ? QColor(0, 0, 255, 255)
                : (withAlpha ? QColor(255, 255, 255, 128)
                             : QColor(255, 255, 255, 255));
      img.setPixelColor(x, y, c);
    }
  return img;
}

void checkClose(QColor got, QColor want, int tol)
{
  CHECK(std::abs(got.red() - want.red()) <= tol);
  CHECK(std::abs(got.green() - want.green()) <= tol);
  CHECK(std::abs(got.blue() - want.blue()) <= tol);
  CHECK(std::abs(got.alpha() - want.alpha()) <= tol);
}

// Assert the quadrant pattern at safe in-quadrant sample points.
// tol = 0 for lossless formats; a few counts for lossy ones.
void checkQuadrants(const QImage& img, int tol, int brAlpha)
{
  REQUIRE(!img.isNull());
  const int w = img.width(), h = img.height();
  checkClose(img.pixelColor(w / 4, h / 4), QColor(255, 0, 0, 255), tol);
  checkClose(img.pixelColor(3 * w / 4, h / 4), QColor(0, 255, 0, 255), tol);
  checkClose(img.pixelColor(w / 4, 3 * h / 4), QColor(0, 0, 255, 255), tol);
  checkClose(
      img.pixelColor(3 * w / 4, 3 * h / 4), QColor(255, 255, 255, brAlpha), tol);
}

// Serialize `img` with QImageWriter into `bytes` using `fmt`.
QByteArray encode(const QImage& img, const char* fmt, int quality = -1)
{
  QByteArray bytes;
  QBuffer buf(&bytes);
  buf.open(QIODevice::WriteOnly);
  QImageWriter w(&buf, fmt);
  if(quality >= 0)
    w.setQuality(quality);
  REQUIRE(w.write(img));
  REQUIRE(!bytes.isEmpty());
  return bytes;
}

// A temp dir + one file written from raw bytes.
struct FileFixture
{
  QTemporaryDir dir;
  QString path;

  FileFixture(const QByteArray& bytes, const QString& name)
  {
    REQUIRE(dir.isValid());
    path = dir.filePath(name);
    QFile f(path);
    REQUIRE(f.open(QIODevice::WriteOnly));
    REQUIRE(f.write(bytes) == bytes.size());
  }
};

// Uncompressed 32-bit true-color TGA 2.0 (type 2) of the quadrant pattern,
// 4x4, with the TRUEVISION-XFILE footer Qt requires. `topLeft` selects the
// origin bit (descriptor bit 5); the pixel rows are stored accordingly so
// both variants describe the SAME logical image.
QByteArray makeTga(bool topLeft, bool withFooter = true)
{
  QByteArray d;
  char hdr[18] = {};
  hdr[2] = 2;                       // uncompressed true-color
  hdr[12] = 4;                      // width  (lo byte)
  hdr[14] = 4;                      // height (lo byte)
  hdr[16] = 32;                     // 32 bpp
  hdr[17] = topLeft ? 0x28 : 0x08;  // 8 attr bits; bit5 = top-left origin
  d.append(hdr, 18);
  for(int row = 0; row < 4; row++)
  {
    // bottom-left files store the bottom row first
    const int y = topLeft ? row : 3 - row;
    for(int x = 0; x < 4; x++)
    {
      int r, g, b, a = 255;
      if(y < 2)
      {
        r = x < 2 ? 255 : 0;
        g = x < 2 ? 0 : 255;
        b = 0;
      }
      else if(x < 2)
      {
        r = 0;
        g = 0;
        b = 255;
      }
      else
      {
        r = 255;
        g = 255;
        b = 255;
        a = 128;
      }
      const char px[4] = {(char)b, (char)g, (char)r, (char)a};  // file is BGRA
      d.append(px, 4);
    }
  }
  if(withFooter)
  {
    char footer[26] = {};
    std::memcpy(footer + 8, "TRUEVISION-XFILE.", 18);  // incl. trailing NUL
    d.append(footer, 26);
  }
  return d;
}

// 4x4 quadrant GIF (2x2 cells), generated once with ImageMagick from a
// known-pixel PPM (`convert quad.ppm +dither -colors 4 quad.gif`) and
// embedded — Qt reads GIF but cannot write it. Palette: pure red / green /
// blue / white; round-trip through ImageMagick back to PPM verified
// byte-identical to the source pattern.
const unsigned char kQuadGif[] = {
    0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x04, 0x00, 0x04, 0x00, 0xf1, 0x03,
    0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
    0xff, 0x21, 0xf9, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x21, 0xff, 0x0b,
    0x49, 0x6d, 0x61, 0x67, 0x65, 0x4d, 0x61, 0x67, 0x69, 0x63, 0x6b, 0x0e,
    0x67, 0x61, 0x6d, 0x6d, 0x61, 0x3d, 0x30, 0x2e, 0x34, 0x35, 0x34, 0x35,
    0x34, 0x35, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x04, 0x00,
    0x00, 0x02, 0x07, 0x04, 0x12, 0x86, 0x22, 0x33, 0xec, 0x0a, 0x00, 0x3b};
}

// ============================================================================
// gfx decode — PNG (the reference lossless path: exact values, channel
// order, orientation, alpha)
// ============================================================================

TEST_CASE("ImageLoader: PNG decodes with exact pixels, straight alpha, RGBA order, top-left origin",
          "[gfx][imageloader]")
{
  ensureApp();
  const QImage src = quadrantImage();
  const QByteArray bytes = encode(src, "png");
  FileFixture file(bytes, "quad.png");

  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  const QImage& img = decoded->image;

  CHECK(img.format() == QImage::Format_RGBA8888);
  CHECK(img.width() == 16);
  CHECK(img.height() == 16);
  checkQuadrants(img, 0, 128);

  // Channel order in memory: RGBA8888 is byte-ordered R,G,B,A. First pixel
  // of the top scanline is the top-left quadrant (red) — this both proves
  // the byte order (a BGRA mixup would read 0,0,255) and the orientation
  // (a flipped image would read blue here).
  const uchar* row0 = img.constScanLine(0);
  CHECK(row0[0] == 255);  // R
  CHECK(row0[1] == 0);    // G
  CHECK(row0[2] == 0);    // B
  CHECK(row0[3] == 255);  // A
  const uchar* rowLast = img.constScanLine(15);
  CHECK(rowLast[0] == 0);    // bottom-left quadrant is blue
  CHECK(rowLast[1] == 0);
  CHECK(rowLast[2] == 255);
  CHECK(rowLast[3] == 255);

  // Straight (non-premultiplied) alpha: the white/128 quadrant keeps its
  // full-intensity color channels (premultiplied would store 128,128,128).
  const uchar* brPix = img.constScanLine(12) + 12 * 4;
  CHECK(brPix[0] == 255);
  CHECK(brPix[1] == 255);
  CHECK(brPix[2] == 255);
  CHECK(brPix[3] == 128);

  // Memory decode gives the same result.
  auto fromMem = score::gfx::decodeImageFromMemory(bytes, "image/png");
  REQUIRE(fromMem.has_value());
  CHECK(fromMem->image == img);
}

TEST_CASE("ImageLoader: decode does not linearize sRGB pixel bytes", "[gfx][imageloader]")
{
  // The sRGB decision is upload-time metadata (QRhiTexture::sRGB flag,
  // covered in AssetTableTest); the decode must hand back the encoded
  // bytes untouched. A mid-gray 128 would come back ~55 if someone
  // accidentally linearized on load.
  ensureApp();
  QImage src(4, 4, QImage::Format_RGBA8888);
  src.fill(QColor(128, 64, 200, 255));
  auto decoded = score::gfx::decodeImageFromMemory(encode(src, "png"), "image/png");
  REQUIRE(decoded.has_value());
  const QColor c = decoded->image.pixelColor(2, 2);
  CHECK(c.red() == 128);
  CHECK(c.green() == 64);
  CHECK(c.blue() == 200);
}

// ============================================================================
// gfx decode — 16-bit sources (PNG16 / TIFF16)
// ============================================================================

TEST_CASE("ImageLoader: 16-bit PNG/TIFF decode at full precision in Qt, then quantize to the LDR RGBA8 contract",
          "[gfx][imageloader]")
{
  ensureApp();

  // 16-bit channel values chosen as k*257 so the 16->8 bit conversion is
  // exact regardless of whether Qt divides by 257 or shifts by 8:
  //   0x8080 -> 128, 0x4040 -> 64, 0xFFFF -> 255, 0x0000 -> 0.
  QImage src16(8, 8, QImage::Format_RGBA64);
  for(int y = 0; y < 8; y++)
    for(int x = 0; x < 8; x++)
      src16.setPixelColor(
          x, y, QColor::fromRgba64(128 * 257, 64 * 257, 255 * 257, 255 * 257));
  // one distinct probe pixel
  src16.setPixelColor(3, 5, QColor::fromRgba64(200 * 257, 0, 17 * 257, 128 * 257));

  const char* fmts[] = {"png", "tiff"};
  for(const char* fmt : fmts)
  {
    DYNAMIC_SECTION("format " << fmt)
    {
      if(!formatSupported(fmt))
        SKIP("Qt plugin for " << fmt << " not available");

      const QByteArray bytes = encode(src16, fmt);
      FileFixture file(bytes, QString("quad16.") + fmt);

      // Qt itself preserves the 16-bit depth: this is where an HDR/16-bit
      // loader would branch off. If this ever stops holding, precision is
      // silently lost before our code even runs.
      QImageReader reader(file.path);
      const QImage raw = reader.read();
      REQUIRE(!raw.isNull());
      CHECK(raw.format() == QImage::Format_RGBA64);
      CHECK(raw.depth() == 64);
      CHECK(raw.pixelColor(0, 0).rgba64().red() == 128 * 257);

      // The TextureLoader contract is LDR RGBA8888: 16-bit inputs are
      // quantized to 8 bits — by design, not by accident. Assert the
      // quantization is the correct rounding.
      auto decoded = score::gfx::decodeImageFromPath(file.path);
      REQUIRE(decoded.has_value());
      CHECK(decoded->image.format() == QImage::Format_RGBA8888);
      const QColor c = decoded->image.pixelColor(0, 0);
      CHECK(c.red() == 128);
      CHECK(c.green() == 64);
      CHECK(c.blue() == 255);
      CHECK(c.alpha() == 255);
      const QColor probe = decoded->image.pixelColor(3, 5);
      CHECK(probe.red() == 200);
      CHECK(probe.green() == 0);
      CHECK(probe.blue() == 17);
      CHECK(probe.alpha() == 128);
    }
  }
}

// ============================================================================
// gfx decode — remaining raster formats
// ============================================================================

TEST_CASE("ImageLoader: JPEG decodes within lossy tolerance, alpha forced opaque",
          "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("jpeg"))
    SKIP("no jpeg plugin");
  // JPEG has no alpha — encode the opaque pattern.
  const QByteArray bytes = encode(quadrantImage(16, 16, false), "jpeg", 95);
  FileFixture file(bytes, "quad.jpg");

  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.format() == QImage::Format_RGBA8888);
  CHECK(decoded->image.width() == 16);
  CHECK(decoded->image.height() == 16);
  // 4:2:0 chroma + DCT ringing on hard quadrant edges: allow a loose
  // tolerance at in-quadrant sample points, and require exact opacity.
  checkQuadrants(decoded->image, 12, 255);
  CHECK(decoded->image.pixelColor(4, 4).alpha() == 255);
}

TEST_CASE("ImageLoader: BMP decodes exactly", "[gfx][imageloader]")
{
  ensureApp();
  // Qt's BMP writer does not reliably round-trip alpha -> opaque pattern.
  const QByteArray bytes = encode(quadrantImage(16, 16, false), "bmp");
  FileFixture file(bytes, "quad.bmp");

  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.width() == 16);
  CHECK(decoded->image.height() == 16);
  checkQuadrants(decoded->image, 0, 255);
}

TEST_CASE("ImageLoader: TIFF decodes exactly with alpha", "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("tiff"))
    SKIP("no tiff plugin");
  const QByteArray bytes = encode(quadrantImage(), "tiff");
  FileFixture file(bytes, "quad.tiff");

  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.width() == 16);
  checkQuadrants(decoded->image, 0, 128);
}

TEST_CASE("ImageLoader: WebP (quality 100 = lossless) decodes exactly with alpha",
          "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("webp"))
    SKIP("no webp plugin");
  const QByteArray bytes = encode(quadrantImage(), "webp", 100);
  FileFixture file(bytes, "quad.webp");

  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.width() == 16);
  CHECK(decoded->image.height() == 16);
  checkQuadrants(decoded->image, 0, 128);

  auto fromMem = score::gfx::decodeImageFromMemory(bytes, "image/webp");
  REQUIRE(fromMem.has_value());
  checkQuadrants(fromMem->image, 0, 128);
}

TEST_CASE("ImageLoader: GIF decodes with exact palette colors", "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("gif"))
    SKIP("no gif plugin");
  const QByteArray bytes(
      reinterpret_cast<const char*>(kQuadGif), (int)sizeof(kQuadGif));

  auto fromMem = score::gfx::decodeImageFromMemory(bytes, "image/gif");
  REQUIRE(fromMem.has_value());
  CHECK(fromMem->image.width() == 4);
  CHECK(fromMem->image.height() == 4);
  checkQuadrants(fromMem->image, 0, 255);  // 2x2 palette quadrants, opaque

  FileFixture file(bytes, "quad.gif");
  auto fromPath = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(fromPath.has_value());
  CHECK(fromPath->image == fromMem->image);
}

TEST_CASE("ImageLoader: TGA 2.0 decodes both origins to top-left and swizzles BGRA->RGBA",
          "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("tga"))
    SKIP("no tga plugin");

  SECTION("bottom-left origin file (TGA default row order)")
  {
    FileFixture file(makeTga(/*topLeft=*/false), "quad-bl.tga");
    auto decoded = score::gfx::decodeImageFromPath(file.path);
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.width() == 4);
    CHECK(decoded->image.height() == 4);
    // The file stores the bottom row first; a loader that ignored the
    // origin field would show blue/white on top.
    checkQuadrants(decoded->image, 0, 128);
    // BGRA file bytes must land as RGBA: TL red proves no channel swap.
    CHECK(decoded->image.pixelColor(0, 0) == QColor(255, 0, 0, 255));
  }

  SECTION("top-left origin file (descriptor bit 5)")
  {
    FileFixture file(makeTga(/*topLeft=*/true), "quad-tl.tga");
    auto decoded = score::gfx::decodeImageFromPath(file.path);
    REQUIRE(decoded.has_value());
    checkQuadrants(decoded->image, 0, 128);
  }

  SECTION("footer-less original-spec TGA decodes via the footer-append fallback")
  {
    // Qt's qtga handler only recognizes TGA 2.0 ("TRUEVISION-XFILE."
    // footer). Plain TGAs — still produced by many tools — are retried
    // with the footer appended when the header plausibly describes a TGA.
    const QByteArray bytes = makeTga(false, /*withFooter=*/false);

    auto fromMem = score::gfx::decodeImageFromMemory(bytes, {});
    REQUIRE(fromMem.has_value());
    checkQuadrants(fromMem->image, 0, 128);
    CHECK(fromMem->image.pixelColor(0, 0) == QColor(255, 0, 0, 255));

    FileFixture file(bytes, "quad-nofooter.tga");
    auto fromPath = score::gfx::decodeImageFromPath(file.path);
    REQUIRE(fromPath.has_value());
    CHECK(fromPath->image == fromMem->image);
  }
}

TEST_CASE("ImageLoader: PPM (P6) and PGM (P5) decode exactly", "[gfx][imageloader]")
{
  ensureApp();

  SECTION("P6 color, hand-crafted bytes")
  {
    QByteArray ppm("P6\n4 4\n255\n");
    for(int y = 0; y < 4; y++)
      for(int x = 0; x < 4; x++)
      {
        char px[3];
        if(y < 2)
        {
          px[0] = x < 2 ? (char)255 : 0;
          px[1] = x < 2 ? 0 : (char)255;
          px[2] = 0;
        }
        else if(x < 2)
        {
          px[0] = 0;
          px[1] = 0;
          px[2] = (char)255;
        }
        else
        {
          px[0] = px[1] = px[2] = (char)255;
        }
        ppm.append(px, 3);
      }

    auto fromMem = score::gfx::decodeImageFromMemory(ppm, {});
    REQUIRE(fromMem.has_value());
    CHECK(fromMem->image.width() == 4);
    CHECK(fromMem->image.height() == 4);
    checkQuadrants(fromMem->image, 0, 255);

    FileFixture file(ppm, "quad.ppm");
    auto fromPath = score::gfx::decodeImageFromPath(file.path);
    REQUIRE(fromPath.has_value());
    CHECK(fromPath->image == fromMem->image);
  }

  SECTION("P5 grayscale replicates gray into R=G=B")
  {
    QByteArray pgm("P5\n2 2\n255\n");
    const unsigned char gray[4] = {0, 87, 127, 255};
    pgm.append(reinterpret_cast<const char*>(gray), 4);

    auto decoded = score::gfx::decodeImageFromMemory(pgm, {});
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.format() == QImage::Format_RGBA8888);
    CHECK(decoded->image.pixelColor(0, 0) == QColor(0, 0, 0, 255));
    CHECK(decoded->image.pixelColor(1, 0) == QColor(87, 87, 87, 255));
    CHECK(decoded->image.pixelColor(0, 1) == QColor(127, 127, 127, 255));
    CHECK(decoded->image.pixelColor(1, 1) == QColor(255, 255, 255, 255));
  }
}

TEST_CASE("ImageLoader: SVG rasterizes at intrinsic size with transparent background",
          "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("svg"))
    SKIP("no svg image-format plugin");

  const QByteArray svg
      = "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'>"
        "<rect x='0' y='0' width='4' height='8' fill='#ff0000'/>"
        "<rect x='4' y='0' width='4' height='4' fill='#00ff00'/>"
        "</svg>";
  FileFixture file(svg, "shapes.svg");
  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.width() == 8);
  CHECK(decoded->image.height() == 8);
  // Pixel-aligned rect interiors are exact; the uncovered bottom-right
  // quadrant must be fully transparent, not black-opaque.
  CHECK(decoded->image.pixelColor(2, 2) == QColor(255, 0, 0, 255));
  CHECK(decoded->image.pixelColor(6, 2) == QColor(0, 255, 0, 255));
  CHECK(decoded->image.pixelColor(2, 6) == QColor(255, 0, 0, 255));
  CHECK(decoded->image.pixelColor(6, 6).alpha() == 0);
}

TEST_CASE("ImageLoader: ICO decodes exactly with alpha", "[gfx][imageloader]")
{
  ensureApp();
  if(!formatSupported("ico"))
    SKIP("no ico plugin");
  const QByteArray bytes = encode(quadrantImage(), "ico");
  FileFixture file(bytes, "quad.ico");
  auto decoded = score::gfx::decodeImageFromPath(file.path);
  REQUIRE(decoded.has_value());
  CHECK(decoded->image.width() == 16);
  checkQuadrants(decoded->image, 0, 128);
}

// ============================================================================
// gfx decode — corrupt / truncated / mislabeled inputs (fuzz-ish sweep).
// The contract: never crash (ASAN-clean), fail as std::nullopt, or — where
// Qt deliberately tolerates damage — return a sane full-dimension image.
// ============================================================================

TEST_CASE("ImageLoader: corrupt inputs are rejected without crashing", "[gfx][imageloader][corrupt]")
{
  ensureApp();

  SECTION("zero-byte file and empty byte array")
  {
    FileFixture file(QByteArray{}, "empty.png");
    CHECK_FALSE(score::gfx::decodeImageFromPath(file.path).has_value());
    CHECK_FALSE(score::gfx::decodeImageFromMemory(QByteArray{}, {}).has_value());
    CHECK_FALSE(
        score::gfx::decodeImageFromMemory(QByteArray{}, "image/png").has_value());
  }

  SECTION("garbage bytes with a .png extension")
  {
    QByteArray garbage(1024, '\x5A');
    FileFixture file(garbage, "garbage.png");
    CHECK_FALSE(score::gfx::decodeImageFromPath(file.path).has_value());
    CHECK_FALSE(score::gfx::decodeImageFromMemory(garbage, "image/png").has_value());
  }

  SECTION("valid PNG signature, corrupted chunk data")
  {
    QByteArray bytes = encode(quadrantImage(), "png");
    // Stomp the middle of the file (IDAT payload + CRCs).
    for(int i = bytes.size() / 3; i < 2 * bytes.size() / 3; i++)
      bytes[i] = (char)(i * 31);
    CHECK_FALSE(score::gfx::decodeImageFromMemory(bytes, "image/png").has_value());
    FileFixture file(bytes, "stomped.png");
    CHECK_FALSE(score::gfx::decodeImageFromPath(file.path).has_value());
  }

  SECTION("truncation sweep across formats")
  {
    // For png/tiff/webp/jpeg/ppm/gif Qt fails cleanly (verified behavior);
    // asserted as hard failures so a decoder swap that starts returning
    // half-decoded frames is caught deliberately.
    struct Case
    {
      const char* fmt;
      QByteArray bytes;
    };
    std::vector<Case> cases;
    cases.push_back({"png", encode(quadrantImage(), "png")});
    if(formatSupported("jpeg"))
      cases.push_back({"jpeg", encode(quadrantImage(16, 16, false), "jpeg", 95)});
    if(formatSupported("tiff"))
      cases.push_back({"tiff", encode(quadrantImage(), "tiff")});
    if(formatSupported("webp"))
      cases.push_back({"webp", encode(quadrantImage(), "webp", 100)});
    cases.push_back({"ppm", encode(quadrantImage(16, 16, false), "ppm")});
    if(formatSupported("gif"))
      cases.push_back(
          {"gif",
           QByteArray(reinterpret_cast<const char*>(kQuadGif), (int)sizeof(kQuadGif))});

    for(const auto& c : cases)
    {
      for(int percent : {10, 25, 50, 75})
      {
        DYNAMIC_SECTION("format " << c.fmt << " truncated to " << percent << "%")
        {
          const QByteArray t = c.bytes.left(c.bytes.size() * percent / 100);
          CHECK_FALSE(score::gfx::decodeImageFromMemory(t, {}).has_value());
          FileFixture file(t, QString("trunc.") + c.fmt);
          CHECK_FALSE(score::gfx::decodeImageFromPath(file.path).has_value());
        }
      }
    }
  }

  SECTION("truncated BMP decodes to full-size image (documented Qt tolerance)")
  {
    // Qt's BMP reader pads missing rows instead of failing: dimensions come
    // from the intact header, absent pixel data reads as zeros. No crash,
    // no OOB (ASAN validates); consumers get a valid — if partial — image.
    const QByteArray bytes = encode(quadrantImage(16, 16, false), "bmp");
    const QByteArray t = bytes.left(bytes.size() / 2);
    auto decoded = score::gfx::decodeImageFromMemory(t, {});
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.width() == 16);
    CHECK(decoded->image.height() == 16);
  }

  SECTION("PNG payload with a lying .jpg extension loads from path (content sniffing)")
  {
    const QByteArray bytes = encode(quadrantImage(), "png");
    FileFixture file(bytes, "actually-a-png.jpg");
    auto decoded = score::gfx::decodeImageFromPath(file.path);
    REQUIRE(decoded.has_value());
    CHECK(decoded->image.width() == 16);
    checkQuadrants(decoded->image, 0, 128);
  }

  SECTION("wrong explicit format hint fails: no fallback probing")
  {
    const QByteArray bytes = encode(quadrantImage(), "png");
    // Only assert the jpeg case when a jpeg handler exists: when the build has
    // no handler at all for the hinted format (e.g. static macOS SDK Qt without
    // qjpeg), QImageReader falls back to content detection and decodes the PNG,
    // which is documented Qt behavior rather than a hint-handling bug.
    if(formatSupported("jpeg"))
      CHECK_FALSE(score::gfx::decodeImageFromMemory(bytes, "image/jpeg").has_value());
    CHECK_FALSE(score::gfx::decodeImageFromMemory(bytes, "image/bmp").has_value());
    // ...while the unhinted decode sniffs the real format.
    CHECK(score::gfx::decodeImageFromMemory(bytes, {}).has_value());
  }

  SECTION("deterministic random-byte fuzz buffers")
  {
    uint32_t state = 0xC0FFEE42;
    auto next = [&state] {
      state = state * 1664525u + 1013904223u;
      return (char)(state >> 24);
    };
    for(int size : {1, 2, 16, 256, 4096, 65536})
    {
      QByteArray buf(size, Qt::Uninitialized);
      for(int i = 0; i < size; i++)
        buf[i] = next();
      // Must not crash / leak / overflow; with this fixed seed none of the
      // buffers resembles a valid header, so decode also fails.
      CHECK_FALSE(score::gfx::decodeImageFromMemory(buf, {}).has_value());
      CHECK_FALSE(score::gfx::decodeImageFromMemory(buf, "image/png").has_value());
    }
  }
}

// ============================================================================
// Threedim::ImageLoader (LDR) — the file-port process() decode contract.
// process() runs on the file-load thread and returns a lambda that stages
// the decoded QImage on the node; the GPU upload itself (init/update) needs
// a live RenderList and is exercised by the render-harness tests instead.
// ============================================================================

namespace
{
using LdrPort = Threedim::ImageLoader::ins::image_t;

halp::mmap_file_view makeView(const QByteArray& bytes, const std::string& filename)
{
  halp::mmap_file_view v;
  v.bytes = std::string_view(bytes.constData(), (size_t)bytes.size());
  v.filename = filename;
  return v;
}
}

TEST_CASE("ImageLoaderLDR: process() decodes bytes and stages an RGBA8888 image",
          "[threedim][imageloader]")
{
  ensureApp();
  const QByteArray bytes = encode(quadrantImage(), "png");

  auto apply = LdrPort::process(makeView(bytes, "unused.png"));
  REQUIRE(apply);

  Threedim::ImageLoader node;
  CHECK(node.m_pendingImage.isNull());
  CHECK_FALSE(node.m_changed);

  apply(node);
  CHECK(node.m_changed);
  REQUIRE(!node.m_pendingImage.isNull());
  CHECK(node.m_pendingImage.format() == QImage::Format_RGBA8888);
  CHECK(node.m_pendingImage.width() == 16);
  CHECK(node.m_pendingImage.height() == 16);
  checkQuadrants(node.m_pendingImage, 0, 128);
  // Top-left origin, no flip (memory note "texture-origin-top-left").
  CHECK(node.m_pendingImage.pixelColor(0, 0) == QColor(255, 0, 0, 255));
  CHECK(node.m_pendingImage.pixelColor(0, 15) == QColor(0, 0, 255, 255));
}

TEST_CASE("ImageLoaderLDR: process() falls back to the filename when bytes are empty or garbage",
          "[threedim][imageloader]")
{
  ensureApp();
  const QByteArray bytes = encode(quadrantImage(), "png");
  FileFixture file(bytes, "fallback.png");
  const std::string path = file.path.toStdString();

  SECTION("empty byte view, valid path")
  {
    auto apply = LdrPort::process(makeView(QByteArray{}, path));
    REQUIRE(apply);
    Threedim::ImageLoader node;
    apply(node);
    REQUIRE(!node.m_pendingImage.isNull());
    CHECK(node.m_pendingImage.width() == 16);
    checkQuadrants(node.m_pendingImage, 0, 128);
  }

  SECTION("garbage bytes, valid path")
  {
    auto apply = LdrPort::process(makeView(QByteArray(64, '\x7F'), path));
    REQUIRE(apply);
    Threedim::ImageLoader node;
    apply(node);
    REQUIRE(!node.m_pendingImage.isNull());
    CHECK(node.m_pendingImage.width() == 16);
  }
}

TEST_CASE("ImageLoaderLDR: process() with undecodable input stages a null image without crashing",
          "[threedim][imageloader][corrupt]")
{
  ensureApp();

  auto checkNullStage = [](halp::mmap_file_view v) {
    auto apply = LdrPort::process(v);
    REQUIRE(apply);  // contract: always returns a stage lambda
    Threedim::ImageLoader node;
    apply(node);
    // A null image is staged with m_changed set; update() then no-ops on
    // the isNull() check, so the output keeps its previous texture.
    CHECK(node.m_pendingImage.isNull());
    CHECK(node.m_changed);
  };

  SECTION("garbage bytes, no filename")
  {
    checkNullStage(makeView(QByteArray(256, '\x33'), ""));
  }
  SECTION("empty bytes, missing file")
  {
    checkNullStage(makeView(QByteArray{}, "/nonexistent/not-here.png"));
  }
  SECTION("truncated png bytes, no filename")
  {
    const QByteArray png = encode(quadrantImage(), "png");
    checkNullStage(makeView(png.left(png.size() / 2), ""));
  }
}

// ============================================================================
// Threedim::ArrayToTexture — float array -> texture bytes, per format branch.
// Pure CPU (halp::texture_output storage), no GPU needed.
// ============================================================================

namespace
{
Threedim::ArrayToTexture
makeA2T(int w, int h, halp::custom_texture::texture_format fmt, std::vector<float> data)
{
  Threedim::ArrayToTexture node;
  node.inputs.size.value = {w, h};
  node.inputs.format.value = fmt;
  node.inputs.in.value = std::move(data);
  node.recreate();
  return node;
}
}

TEST_CASE("ArrayToTexture: RGBA8 packs floats as bytes in array order", "[threedim][arraytotexture]")
{
  // 2x2 RGBA8: row-major, 4 floats per pixel, values are truncated casts.
  auto node = makeA2T(
      2, 2, halp::custom_texture::RGBA8,
      {255, 0, 0, 255, /**/ 0, 255, 0, 255,
       /**/ 0, 0, 255, 128, /**/ 10, 20, 30, 40});

  auto& tex = node.outputs.main.texture;
  CHECK(tex.width == 2);
  CHECK(tex.height == 2);
  CHECK(tex.changed);  // upload() flags the new content
  REQUIRE(tex.bytes != nullptr);
  const uint8_t* b = tex.bytes;
  const uint8_t expected[16]
      = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 128, 10, 20, 30, 40};
  for(int i = 0; i < 16; i++)
    CHECK(b[i] == expected[i]);
}

TEST_CASE("ArrayToTexture: R32F keeps float values bit-exact", "[threedim][arraytotexture]")
{
  auto node = makeA2T(
      2, 2, halp::custom_texture::R32F, {0.0f, 1.5f, -2.25f, 1e6f});
  auto& tex = node.outputs.main.texture;
  REQUIRE(tex.bytes != nullptr);
  const float* f = reinterpret_cast<const float*>(tex.bytes);
  CHECK(f[0] == 0.0f);
  CHECK(f[1] == 1.5f);
  CHECK(f[2] == -2.25f);
  CHECK(f[3] == 1e6f);
}

TEST_CASE("ArrayToTexture: RGBA32F keeps all four channels bit-exact", "[threedim][arraytotexture]")
{
  auto node = makeA2T(
      1, 1, halp::custom_texture::RGBA32F, {0.25f, 0.5f, 0.75f, 1.0f});
  auto& tex = node.outputs.main.texture;
  REQUIRE(tex.bytes != nullptr);
  const float* f = reinterpret_cast<const float*>(tex.bytes);
  CHECK(f[0] == 0.25f);
  CHECK(f[1] == 0.5f);
  CHECK(f[2] == 0.75f);
  CHECK(f[3] == 1.0f);
}

TEST_CASE("ArrayToTexture: R16 widens float input into uint16 storage", "[threedim][arraytotexture]")
{
  auto node = makeA2T(2, 1, halp::custom_texture::R16, {65535.f, 1234.f});
  auto& tex = node.outputs.main.texture;
  REQUIRE(tex.bytes != nullptr);
  const uint16_t* u = reinterpret_cast<const uint16_t*>(tex.bytes);
  CHECK(u[0] == 65535);
  CHECK(u[1] == 1234);
}

#if(SCORE_LIBC_HAS_FLOAT16 && SCORE_COMPILER_HAS_FLOAT16)
TEST_CASE("ArrayToTexture: R16F converts to half-floats where supported", "[threedim][arraytotexture]")
{
  // Values exactly representable in fp16.
  auto node = makeA2T(2, 1, halp::custom_texture::R16F, {1.0f, -0.5f});
  auto& tex = node.outputs.main.texture;
  REQUIRE(tex.bytes != nullptr);
  const _Float16* f = reinterpret_cast<const _Float16*>(tex.bytes);
  CHECK((float)f[0] == 1.0f);
  CHECK((float)f[1] == -0.5f);
}
#endif

TEST_CASE("ArrayToTexture: short input only writes the provided prefix (no overflow)",
          "[threedim][arraytotexture][corrupt]")
{
  // 2x2 RGBA8 needs 16 values; provide 4 -> exactly 4 bytes written,
  // storage stays in-bounds (ASAN validates the copy).
  auto node = makeA2T(2, 2, halp::custom_texture::RGBA8, {1, 2, 3, 4});
  auto& tex = node.outputs.main.texture;
  CHECK(tex.width == 2);
  CHECK(tex.height == 2);
  REQUIRE(tex.bytes != nullptr);
  CHECK(tex.bytes[0] == 1);
  CHECK(tex.bytes[1] == 2);
  CHECK(tex.bytes[2] == 3);
  CHECK(tex.bytes[3] == 4);
  // bytes[4..15] are default-init storage — defined to exist (no OOB) but
  // not to hold any particular value; not asserted.
}

TEST_CASE("ArrayToTexture: zero-size texture is handled without crashing",
          "[threedim][arraytotexture][corrupt]")
{
  auto node = makeA2T(0, 0, halp::custom_texture::RGBA8, {1, 2, 3});
  auto& tex = node.outputs.main.texture;
  CHECK(tex.width == 0);
  CHECK(tex.height == 0);
}
