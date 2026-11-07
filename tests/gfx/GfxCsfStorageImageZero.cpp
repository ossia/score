// Storage images a compute shader loads before it stores into them start at
// zero.
//
// * zeroTexture, the helper RenderedCSFNode uses for read_write and
//   PERSISTENT images and their history copies: a texture of each layout
//   (mipmapped 2D, array, cube, 3D) and of a float format is filled with
//   magenta, then zeroed, then read back level by level and layer by layer.
//   Every texel must be 0 on every backend, whatever the driver does with
//   fresh memory.
// * end to end: csf-read-before-store.cs adds a quarter of green to what its
//   read_write image holds. Storage images of the same size and format are
//   filled with magenta and freed right before, so a driver recycling memory
//   hands them back; the first frame must read (0, 64, 0) regardless.
#include <score_test/Gfx.hpp>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <string>
#include <vector>

using namespace score::test::gfx;

namespace
{
struct Layout
{
  const char* name;
  QRhiTexture::Format format;
  QRhiTexture::Flags flags;
  int layers;
  int depth;
  //! Past 256, more than one of zeroTexture's tiles cover a level.
  int edge{16};
};

int bytes_per_pixel(QRhiTexture::Format f)
{
  switch(f)
  {
    case QRhiTexture::RGBA8:
      return 4;
    case QRhiTexture::RGBA16F:
      return 8;
    default:
      return 0;
  }
}

QByteArray magenta(QRhiTexture::Format f, QSize sz)
{
  QByteArray texel;
  if(f == QRhiTexture::RGBA8)
  {
    texel = QByteArray("\xff\x00\xff\xff", 4);
  }
  else
  {
    const uint16_t one = 0x3c00;
    const uint16_t h[4]{one, 0, one, one};
    texel = QByteArray(reinterpret_cast<const char*>(h), sizeof(h));
  }
  QByteArray data;
  for(int i = 0; i < sz.width() * sz.height(); ++i)
    data.append(texel);
  return data;
}

struct ZeroResult
{
  bool skipped = false;
  std::string skip_reason, backend, error;
  //! Readbacks that held a non-zero byte, as "level/layer"
  std::vector<std::string> dirty;
  int subresources = 0;
  bool garbage_seen = false;
};

ZeroResult zero_texture(score::gfx::GraphicsApi be, const Layout& l)
{
  ZeroResult r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    GfxPipeline p;
    p.addSink({8, 8});
    if(!p.create(be))
    {
      r.skipped = true;
      r.skip_reason = p.skipped() ? p.skipReason() : p.error();
      return;
    }
    r.backend = p.backend();
    QRhi& rhi = *p.sink(0)->renderState()->rhi;
    if(l.flags.testFlag(QRhiTexture::TextureArray)
       && !rhi.isFeatureSupported(QRhi::TextureArrays))
    {
      r.skipped = true;
      r.skip_reason = "no texture arrays";
      return;
    }
    if(l.flags.testFlag(QRhiTexture::ThreeDimensional)
       && !(rhi.isFeatureSupported(QRhi::ThreeDimensionalTextures)
            && rhi.isFeatureSupported(QRhi::TextureArrays)))
    {
      // QRhi reads a 3D slice back only where it has both.
      r.skipped = true;
      r.skip_reason = "no 3D texture slice readback";
      return;
    }
    if(!rhi.isTextureFormatSupported(l.format, l.flags))
    {
      r.skipped = true;
      r.skip_reason = "format not supported";
      return;
    }

    const QSize size{l.edge, l.edge};
    std::unique_ptr<QRhiTexture> tex;
    if(l.flags.testFlag(QRhiTexture::ThreeDimensional))
      tex.reset(rhi.newTexture(l.format, l.edge, l.edge, l.depth, 1, l.flags));
    else if(l.flags.testFlag(QRhiTexture::TextureArray))
      tex.reset(rhi.newTextureArray(l.format, l.layers, size, 1, l.flags));
    else
      tex.reset(rhi.newTexture(l.format, size, 1, l.flags));
    if(!tex->create())
    {
      r.error = "texture creation failed";
      return;
    }

    const bool is3D = l.flags.testFlag(QRhiTexture::ThreeDimensional);
    const int levels
        = l.flags.testFlag(QRhiTexture::MipMapped) ? rhi.mipLevelsForSize(size) : 1;
    struct Sub
    {
      int level, layer;
    };
    std::vector<Sub> subs;
    for(int level = 0; level < levels; ++level)
    {
      const int count = is3D ? std::max(1, l.depth >> level) : l.layers;
      for(int layer = 0; layer < count; ++layer)
        subs.push_back({level, layer});
    }
    r.subresources = int(subs.size());

    const auto upload_garbage = [&](QRhiResourceUpdateBatch& res) {
      QVarLengthArray<QRhiTextureUploadEntry, 16> entries;
      for(auto s : subs)
      {
        QRhiTextureSubresourceUploadDescription d(
            magenta(l.format, rhi.sizeForMipLevel(s.level, size)));
        entries.append(QRhiTextureUploadEntry(s.layer, s.level, d));
      }
      QRhiTextureUploadDescription up;
      up.setEntries(entries.cbegin(), entries.cend());
      res.uploadTexture(tex.get(), up);
    };
    const auto read_all = [&](QRhiResourceUpdateBatch& res,
                              std::vector<QRhiReadbackResult>& out) {
      out.resize(subs.size());
      for(std::size_t i = 0; i < subs.size(); ++i)
      {
        QRhiReadbackDescription d(tex.get());
        d.setLevel(subs[i].level);
        d.setLayer(subs[i].layer);
        res.readBackTexture(d, &out[i]);
      }
    };

    // Frame 1: garbage, read back, proves the upload reaches every
    // subresource. Frame 2: zero, read back.
    std::vector<QRhiReadbackResult> before, after;
    for(int frame = 0; frame < 2; ++frame)
    {
      QRhiCommandBuffer* cb{};
      if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
      {
        r.error = "beginOffscreenFrame failed";
        return;
      }
      auto* res = rhi.nextResourceUpdateBatch();
      if(frame == 0)
      {
        upload_garbage(*res);
        read_all(*res, before);
      }
      else
      {
        if(!score::gfx::zeroTexture(rhi, *res, *tex))
        {
          res->release();
          rhi.endOffscreenFrame();
          r.error = "zeroTexture refused the format";
          return;
        }
        read_all(*res, after);
      }
      cb->resourceUpdate(res);
      rhi.endOffscreenFrame();
    }

    r.garbage_seen = true;
    for(auto& b : before)
      if(b.data.isEmpty() || b.data.count('\0') == b.data.size())
        r.garbage_seen = false;
    for(std::size_t i = 0; i < after.size(); ++i)
    {
      const auto& d = after[i].data;
      if(d.isEmpty() || d.count('\0') != d.size())
        r.dirty.push_back(
            std::to_string(subs[i].level) + "/" + std::to_string(subs[i].layer));
    }
  });
  return r;
}

// Fills storage images like the one csf-read-before-store.cs declares with
// magenta and frees them, so the next allocation of that size and format on a
// driver that recycles memory starts out holding it.
void leave_garbage_behind(score::gfx::GraphicsApi be)
{
  GfxPipeline p;
  p.addSink({8, 8});
  if(!p.create(be))
    return;
  QRhi& rhi = *p.sink(0)->renderState()->rhi;
  std::vector<std::unique_ptr<QRhiTexture>> textures;
  QRhiCommandBuffer* cb{};
  if(rhi.beginOffscreenFrame(&cb) != QRhi::FrameOpSuccess)
    return;
  auto* res = rhi.nextResourceUpdateBatch();
  for(int i = 0; i < 8; ++i)
  {
    auto* t = rhi.newTexture(
        QRhiTexture::RGBA8, {16, 16}, 1, QRhiTexture::UsedWithLoadStore);
    if(!t->create())
    {
      delete t;
      break;
    }
    textures.emplace_back(t);
    res->uploadTexture(
        t, QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(
                                            magenta(QRhiTexture::RGBA8, {16, 16}))));
  }
  cb->resourceUpdate(res);
  rhi.endOffscreenFrame();
}
}

TEST_CASE("zeroTexture clears every level and layer", "[gfx][csf][zero]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  const auto layout = GENERATE(
      Layout{"2D mipmapped", QRhiTexture::RGBA8,
             QRhiTexture::MipMapped | QRhiTexture::UsedWithLoadStore, 1, 1},
      Layout{"2D rgba16f", QRhiTexture::RGBA16F, QRhiTexture::UsedWithLoadStore, 1, 1},
      Layout{"array of 3", QRhiTexture::RGBA8,
             QRhiTexture::TextureArray | QRhiTexture::UsedWithLoadStore, 3, 1},
      Layout{"cube map", QRhiTexture::RGBA8, QRhiTexture::CubeMap, 6, 1},
      Layout{"3D, 4 slices", QRhiTexture::RGBA8,
             QRhiTexture::ThreeDimensional | QRhiTexture::UsedWithLoadStore, 1, 4},
      Layout{"2D mipmapped, 300 wide", QRhiTexture::RGBA8,
             QRhiTexture::MipMapped | QRhiTexture::UsedWithLoadStore, 1, 1, 300},
      Layout{"3D, 300 wide, 3 slices", QRhiTexture::RGBA8,
             QRhiTexture::ThreeDimensional | QRhiTexture::UsedWithLoadStore, 1, 3, 300});
  CAPTURE(backend_name(be), layout.name);
  REQUIRE(bytes_per_pixel(layout.format) > 0);

  const ZeroResult r = zero_texture(be, layout);
  if(r.skipped)
    SKIP(r.skip_reason);
  CAPTURE(r.backend, r.dirty);
  REQUIRE(r.error.empty());
  REQUIRE(r.subresources > 0);
  REQUIRE(r.garbage_seen);
  CHECK(r.dirty.empty());
}

TEST_CASE(
    "a read_write storage image loads zeros before its first store",
    "[gfx][csf][zero]")
{
  const auto be = GENERATE(from_range(platform_backends()));
  CAPTURE(backend_name(be));
  if(const char* why = compute_shader_skip_reason(be))
    SKIP(std::string{backend_name(be)} + ": " + why);

  struct
  {
    bool skipped{};
    std::string skip_reason, error;
    std::vector<std::array<int, 4>> frames;
  } r;
  score::test::run_in_gui_app([&](const score::GUIApplicationContext&) {
    leave_garbage_behind(be);

    GfxPipeline p;
    const int cs = p.addCsf(
        QStringLiteral(GFX_TEST_CORPUS_DIR "/csf-read-before-store.cs"));
    if(cs < 0)
    {
      r.error = p.error();
      return;
    }
    const int sink = p.addSink({16, 16});
    p.wire(p.imageOut(cs, 0), p.sinkInput(sink));
    if(!p.create(be))
    {
      r.skipped = p.skipped();
      r.skip_reason = p.skipReason();
      r.error = r.skipped ? std::string{} : p.error();
      return;
    }
    for(int f = 0; f < 2; ++f)
    {
      p.render(1);
      const auto img = p.readback(sink);
      if(!img.valid())
      {
        r.error = "no readback";
        return;
      }
      const auto c = img.center();
      r.frames.push_back({c[0], c[1], c[2], c[3]});
    }
    if(r.error.empty())
      r.error = p.error();
  });
  if(r.skipped)
    SKIP(r.skip_reason);
  INFO(r.error);
  REQUIRE(r.error.empty());
  REQUIRE(r.frames.size() == 2);
  CAPTURE(r.frames[0], r.frames[1]);

  // A quarter of 255 per frame, from zero.
  CHECK(r.frames[0][0] <= 2);
  CHECK(std::abs(r.frames[0][1] - 64) <= 2);
  CHECK(r.frames[0][2] <= 2);
  CHECK(std::abs(r.frames[1][1] - 128) <= 2);
}
