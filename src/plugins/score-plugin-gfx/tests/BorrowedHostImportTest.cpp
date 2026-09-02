// Unit tests for Gfx/Graph/interop/BorrowedHostImportCapture.hpp — the
// zero-copy rung that points the GPU at the producer's own buffers
// (VK_EXT_external_memory_host / D3D12 OpenExistingHeapFromAddress).
//
// The rung's slot arbitration (BorrowedSlotTracker) is covered by
// CaptureSlotLifetimeTest; what is untested is the strategy wrapped around it:
// the refusal ladder init() walks before it imports anything. Each rung of
// that ladder protects a different party — a plane overrun protects the
// producer's memory, the two-buffer floor protects the device's queue, and
// the backend probe is what makes "degrade to the CPU rung" honest rather
// than an engaged strategy that uploads nothing.
//
// QRhi's Null backend everywhere: it answers "no host-import path" exactly
// like GL/D3D11 do, so the DEGRADE decision is real; nothing here concludes
// anything about an actual import, which needs Vulkan or D3D12.
//
// init() reports which rung of the ladder refused only through qDebug, so the
// refusal REASON is captured through a message handler — on the Null backend
// every path ends in `false`, and the reason is what tells a config refused
// for the right reason from one refused for a later, wrong one.

#include <Gfx/Graph/interop/BorrowedHostImportCapture.hpp>

#include <QGuiApplication>

#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
// Before 6.6 the rhi/ headers do not exist and qrhi_p.h declares only the
// base QRhiInitParams; the concrete ones live in the per-backend headers.
#include <QtGui/private/qrhinull_p.h>
#endif

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace score::gfx::interop;

namespace
{
void ensureApp()
{
  if(!qApp)
  {
    static int argc = 1;
    static char arg0[] = "BorrowedHostImportTest";
    static char* argv[] = {arg0, nullptr};
    static QGuiApplication app(argc, argv);
  }
}

std::unique_ptr<QRhi> makeNullRhi()
{
  ensureApp();
  QRhiNullInitParams params;
  return std::unique_ptr<QRhi>(QRhi::create(QRhi::Null, &params));
}

// Captures everything qDebug says while in scope, so a refusal can be pinned
// to ITS reason and not a later rung's.
struct LogCapture
{
  static inline std::string log;
  QtMessageHandler previous{};

  LogCapture()
  {
    log.clear();
    previous = qInstallMessageHandler(
        [](QtMsgType, const QMessageLogContext&, const QString& msg) {
          log += msg.toStdString();
          log += '\n';
        });
  }
  ~LogCapture() { qInstallMessageHandler(previous); }

  bool said(std::string_view what) const
  {
    return log.find(what) != std::string::npos;
  }
};

// A producer-side frame buffer; alignment does not matter on the Null
// backend, which refuses before looking at the pointers.
std::vector<BorrowedHostBuffer> buffers(std::size_t n, std::size_t bytes)
{
  static std::vector<std::unique_ptr<std::uint8_t[]>> storage;
  std::vector<BorrowedHostBuffer> v;
  for(std::size_t i = 0; i < n; ++i)
  {
    storage.push_back(std::make_unique<std::uint8_t[]>(bytes));
    BorrowedHostBuffer b;
    b.host = storage.back().get();
    b.bytes = bytes;
    v.push_back(b);
  }
  return v;
}
} // namespace

TEST_CASE("the strategy names itself after its producer")
{
  CHECK(
      std::string(BorrowedHostImportCapture{"v4l2", {}}.name())
      == "v4l2-hostimport-borrowed");
  // A backend handing a null tag is a bug, but must not crash the name.
  CHECK(
      std::string(BorrowedHostImportCapture{nullptr, {}}.name())
      == "?-hostimport-borrowed");
}

TEST_CASE("a config without the essentials is refused")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  std::unique_ptr<QRhiTexture> tex{
      rhi->newTexture(QRhiTexture::RGBA8, QSize(4, 4))};
  REQUIRE(tex->create());

  BorrowedHostImportCapture s{"test", buffers(4, 64)};

  VideoCaptureStrategyConfig cfg;
  cfg.rhi = nullptr;
  cfg.outputTexture = tex.get();
  cfg.frameByteSize = 64;
  CHECK_FALSE(s.init(cfg));

  cfg.rhi = rhi.get();
  cfg.outputTexture = nullptr;
  CHECK_FALSE(s.init(cfg));

  cfg.outputTexture = tex.get();
  cfg.frameByteSize = 0;
  CHECK_FALSE(s.init(cfg));
}

TEST_CASE("a plane that would overrun the producer's buffer is refused first")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);

  // An NV12-shaped decoder: 4x4 luma + 2x2 RG chroma = 16 + 8 bytes.
  std::unique_ptr<QRhiTexture> luma{rhi->newTexture(QRhiTexture::R8, QSize(4, 4))};
  std::unique_ptr<QRhiTexture> chroma{
      rhi->newTexture(QRhiTexture::RG8, QSize(2, 2))};
  REQUIRE(luma->create());
  REQUIRE(chroma->create());

  BorrowedHostImportCapture s{"test", buffers(4, 20)};

  VideoCaptureStrategyConfig cfg;
  cfg.rhi = rhi.get();
  cfg.width = 4;
  cfg.height = 4;
  cfg.outputTexture = luma.get();
  cfg.planes = {luma.get(), chroma.get()};
  // 20 bytes cannot hold 16 luma + 8 chroma: reading chroma would run 4 bytes
  // into memory the producer never lent.
  cfg.frameByteSize = 20;

  LogCapture cap;
  CHECK_FALSE(s.init(cfg));
  CHECK(cap.said("plane overruns the frame"));
}

TEST_CASE("a null decoder plane texture is refused, not dereferenced")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  std::unique_ptr<QRhiTexture> luma{rhi->newTexture(QRhiTexture::R8, QSize(4, 4))};
  REQUIRE(luma->create());

  BorrowedHostImportCapture s{"test", buffers(4, 64)};

  VideoCaptureStrategyConfig cfg;
  cfg.rhi = rhi.get();
  cfg.outputTexture = luma.get();
  cfg.frameByteSize = 24;
  cfg.planes = {luma.get(), nullptr};

  LogCapture cap;
  CHECK_FALSE(s.init(cfg));
  CHECK(cap.said("plane texture is null"));
}

TEST_CASE("fewer than two producer buffers cannot ring")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  std::unique_ptr<QRhiTexture> tex{
      rhi->newTexture(QRhiTexture::RGBA8, QSize(4, 4))};
  REQUIRE(tex->create());

  VideoCaptureStrategyConfig cfg;
  cfg.rhi = rhi.get();
  cfg.outputTexture = tex.get();
  cfg.frameByteSize = 64;

  {
    BorrowedHostImportCapture s{"test", buffers(1, 64)};
    LogCapture cap;
    CHECK_FALSE(s.init(cfg));
    CHECK(cap.said("buffers"));
    CHECK_FALSE(cap.said("no host-import path"));
  }
  {
    BorrowedHostImportCapture s{"test", {}};
    CHECK_FALSE(s.init(cfg));
  }
}

TEST_CASE("a backend with no host-import path degrades honestly")
{
  auto rhi = makeNullRhi();
  REQUIRE(rhi);
  std::unique_ptr<QRhiTexture> tex{
      rhi->newTexture(QRhiTexture::RGBA8, QSize(4, 4))};
  REQUIRE(tex->create());

  auto bufs = buffers(4, 64);
  BorrowedHostImportCapture s{"test", bufs};

  VideoCaptureStrategyConfig cfg;
  cfg.rhi = rhi.get();
  cfg.width = 4;
  cfg.height = 4;
  cfg.outputTexture = tex.get();
  cfg.frameByteSize = 64;

  LogCapture cap;
  // The config is otherwise sound — this refusal is the Null/GL/D3D11 answer,
  // which is what sends the renderer down to the CPU rung.
  CHECK_FALSE(s.init(cfg));
  CHECK(cap.said("no host-import path"));

  // A refused strategy still answers its accessors sanely...
  CHECK(s.slotCount() == 4);
  CHECK(s.slotBuffer(0) == bufs[0].host);
  CHECK(s.slotBuffer(4) == nullptr);
  CHECK(s.outputTexture() == tex.get());

  // ...refuses an out-of-range producer slot...
  CHECK_FALSE(s.ingestFrame(4));
  CHECK(s.ingestFrame(0));

  // ...has lent nothing back yet, and can be released without having engaged.
  CHECK(s.takeReturnedSlots() == 0u);
  s.release();
}
