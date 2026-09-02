// Threedim::InjectTexture + Threedim::ExtractTexture + Threedim::TextureToBuffer
// — the aux-texture trio around the scene cable: inject a live texture handle
// into a scene_state under a name, find it back by name on the geometry side,
// and mirror a CPU texture into a GPU buffer.
//
// All CPU, no GPU, no display:
//  - InjectTexture is pure scene_state algebra (rebuild()/operator()()).
//  - ExtractTexture::update()/release() never touch their RenderList& /
//    QRhiResourceUpdateBatch& parameters (both are commented out in the
//    signatures), so we hand them references into inert storage they are
//    contractually forbidden to dereference — the exact seam
//    tests/threedim/CameraRelease.cpp documents. The QRhiTexture metadata it
//    reads (flags/pixelSize/depth/arraySize/format) are all inline
//    non-virtual getters over protected members, and QRhiResource's
//    ctor/dtor are safe with a null QRhiImplementation* (ctor stores the
//    pointer + an atomic id; dtor is empty), so a stub QRhiTexture subclass
//    makes the whole shape/format taxonomy CPU-reachable.
//  - TextureToBuffer's guarded early-outs in init()/update() return before
//    the first `renderer.` use; only those paths are driven. A stub
//    QRhiBuffer provides size() (inline member read) for update()'s
//    size-comparison arithmetic.
//
// Out of scope (real render infrastructure, not a unit seam):
//  - ExtractBuffer2 itself (covered by ExtractIndex/ExtractComputeSrb).
//  - TextureToBuffer's newBuffer/create/uploadStaticBuffer success path and
//    the releaseBuffer-on-resize path — both dereference a live RenderList.
//  - ExtractTexture's deliberate same-pointer short-circuit means an
//    in-place resized QRhiTexture (same pointer, new pixelSize) keeps stale
//    metadata downstream; the source comments document this as an SRB-
//    rebuild-avoidance tradeoff, so it is not asserted against here.

#include <Threedim/ExtractTexture.hpp>
#include <Threedim/InjectTexture.hpp>
#include <Threedim/TextureToBuffer.hpp>

#include <QtGui/private/qrhi_p.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace
{
//! Reference into inert, correctly-aligned storage for parameters the callee
//! provably never dereferences (same seam as tests/threedim/CameraRelease.cpp).
template <typename T>
T& inert_ref()
{
  alignas(std::max_align_t) static unsigned char storage[256]{};
  return *reinterpret_cast<T*>(&storage[0]);
}

//! Stub QRhiTexture: only the inline metadata getters ExtractTexture reads
//! are live. Never registered with any QRhi; pure CPU object.
struct FakeRhiTexture final : QRhiTexture
{
  FakeRhiTexture(
      QRhiTexture::Format fmt, QSize px, int depth, int arraySize,
      QRhiTexture::Flags f)
      : QRhiTexture(nullptr, fmt, px, depth, arraySize, 1, f)
  {
  }
  QRhiResource::Type resourceType() const override { return QRhiResource::Texture; }
  void destroy() override { }
  bool create() override { return true; }
};

//! Stub QRhiBuffer: only size() (inline member read) is live.
struct FakeRhiBuffer final : QRhiBuffer
{
  explicit FakeRhiBuffer(quint32 sz)
      : QRhiBuffer(
            nullptr, QRhiBuffer::Static,
            QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer, sz)
  {
  }
  QRhiResource::Type resourceType() const override { return QRhiResource::Buffer; }
  void destroy() override { }
  bool create() override { return true; }
};

std::shared_ptr<ossia::scene_state> make_state(int64_t version = 1)
{
  auto s = std::make_shared<ossia::scene_state>();
  s->roots = std::make_shared<std::vector<ossia::scene_node_ptr>>();
  s->version = version;
  return s;
}

const ossia::aux_inject_texture*
find_injected(const ossia::scene_state& s, const std::string& name)
{
  const ossia::aux_inject_texture* found = nullptr;
  for(const auto& at : s.inject_textures)
    if(at.name == name)
    {
      if(found)
        FAIL("duplicate inject_textures entry for '" << name << "'");
      found = &at;
    }
  return found;
}

void run_extract(Threedim::ExtractTexture& n)
{
  n.update(
      inert_ref<score::gfx::RenderList>(), inert_ref<QRhiResourceUpdateBatch>(),
      nullptr);
}
} // namespace

// ================================================================ InjectTexture

TEST_CASE(
    "InjectTexture attaches the named handle to a copied scene_state",
    "[threedim][inject]")
{
  int upstream_tex{}, live_tex{};
  auto raw = make_state(5);
  raw->inject_textures.push_back({.name = "existing", .native_handle = &upstream_tex});
  raw->materials = std::make_shared<std::vector<ossia::material_component_ptr>>();

  Threedim::InjectTexture n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.texture.texture.handle = &live_tex;
  n.inputs.aux_name.value = "base_color_dyn0";
  n();

  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  CHECK(out != raw);
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  // The addressed slot got the handle; pre-existing injections survive.
  REQUIRE(out->inject_textures.size() == 2);
  const auto* mine = find_injected(*out, "base_color_dyn0");
  REQUIRE(mine);
  CHECK(mine->native_handle == &live_tex);
  const auto* other = find_injected(*out, "existing");
  REQUIRE(other);
  CHECK(other->native_handle == &upstream_tex);

  // Fresh version/dirty stamps on the copy.
  CHECK(out->version == 1);
  CHECK(out->dirty_index == 1);

  // Copy-on-write: the upstream state is never mutated...
  CHECK(raw->inject_textures.size() == 1);
  CHECK(raw->version == 5);
  // ...and the shared sub-structures ride along by identity, not by clone.
  CHECK(out->materials.get() == raw->materials.get());
  CHECK(out->roots.get() == raw->roots.get());
}

TEST_CASE(
    "InjectTexture replaces a same-name entry instead of duplicating it",
    "[threedim][inject]")
{
  int old_tex{}, new_tex{};
  auto raw = make_state();
  raw->inject_textures.push_back({.name = "tex", .native_handle = &old_tex});

  Threedim::InjectTexture n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.texture.texture.handle = &new_tex;
  n.inputs.aux_name.value = "tex";
  n();

  const auto out = n.outputs.scene_out.scene.state;
  REQUIRE(out);
  REQUIRE(out->inject_textures.size() == 1);
  const auto* e = find_injected(*out, "tex"); // FAILs on a duplicate
  REQUIRE(e);
  CHECK(e->native_handle == &new_tex);
}

TEST_CASE(
    "InjectTexture ticks: idle republish, upstream bump, handle swap, rename",
    "[threedim][inject]")
{
  int tex_a{}, tex_b{};
  auto raw = make_state(5);

  Threedim::InjectTexture n;
  n.inputs.scene_in.scene.state = raw;
  n.inputs.texture.texture.handle = &tex_a;
  n.inputs.aux_name.value = "aux";
  n();
  const auto first = n.outputs.scene_out.scene.state;
  REQUIRE(first);
  CHECK(n.outputs.scene_out.dirty == 0xFF);

  SECTION("an idle tick republishes the cached state and does not re-dirty")
  {
    n();
    CHECK(n.outputs.scene_out.scene.state == first);
    CHECK(n.outputs.scene_out.dirty == 0);
    CHECK(n.outputs.scene_out.scene.state->version == first->version);
  }

  SECTION("an upstream version bump rebuilds on the new input")
  {
    raw->version = 6;
    n();
    CHECK(n.outputs.scene_out.scene.state != first);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    CHECK(n.outputs.scene_out.scene.state->version == first->version + 1);
  }

  SECTION("a swapped native handle re-addresses the entry")
  {
    n.inputs.texture.texture.handle = &tex_b;
    n();
    const auto out = n.outputs.scene_out.scene.state;
    REQUIRE(out);
    CHECK(out != first);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    const auto* e = find_injected(*out, "aux");
    REQUIRE(e);
    CHECK(e->native_handle == &tex_b);
  }

  SECTION("a rename via the control callback moves the injection, no stale entry")
  {
    n.inputs.aux_name.value = "renamed";
    n.inputs.aux_name.update(n); // the port-driven rebuild the host performs
    n();
    const auto out = n.outputs.scene_out.scene.state;
    REQUIRE(out);
    CHECK(out != first);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
    REQUIRE(out->inject_textures.size() == 1);
    CHECK(find_injected(*out, "aux") == nullptr); // rebuilt from upstream, not from the old output
    const auto* e = find_injected(*out, "renamed");
    REQUIRE(e);
    CHECK(e->native_handle == &tex_a);
  }

  SECTION("clearing the name via the callback reverts to passthrough")
  {
    n.inputs.aux_name.value = "";
    n.inputs.aux_name.update(n);
    n();
    CHECK(n.outputs.scene_out.scene.state == raw);
    CHECK(n.outputs.scene_out.dirty == 0xFF);
  }

  SECTION("producer death (handle goes null) degrades to passthrough")
  {
    n.inputs.texture.texture.handle = nullptr;
    n();
    CHECK(n.outputs.scene_out.scene.state == raw);
    CHECK(raw->inject_textures.empty()); // still pristine
  }
}

TEST_CASE(
    "InjectTexture with no usable inputs passes the scene through untouched",
    "[threedim][inject]")
{
  int tex{};
  auto raw = make_state(3);

  Threedim::InjectTexture n;
  n.inputs.scene_in.scene.state = raw;

  SECTION("null handle") { n.inputs.aux_name.value = "aux"; }
  SECTION("empty name") { n.inputs.texture.texture.handle = &tex; }

  n();
  CHECK(n.outputs.scene_out.scene.state == raw);
  CHECK(n.outputs.scene_out.dirty == 0xFF); // first publish of the passthrough
  n();
  CHECK(n.outputs.scene_out.scene.state == raw);
  CHECK(n.outputs.scene_out.dirty == 0); // and quiet afterwards
}

// DEFECT: operator()() uses `!m_cached_out` as its "never built" trigger, but
// the passthrough of a NULL input scene caches nullptr — so every tick with no
// upstream scene re-enters rebuild() and re-arms m_pending_dirty = 0xFF. A
// disconnected InjectTexture spams dirty=0xFF downstream forever (the TagAs
// 8ad12fe91a re-dirty class; contrast Transform3D, whose null-input ticks
// publish dirty == 0 — see Transform3DCompose.cpp). Correct behaviour is
// asserted; the tag flips this to a reminder the day it is fixed.
TEST_CASE(
    "InjectTexture does not re-dirty every tick while the input scene is null",
    "[threedim][inject][!shouldfail]")
{
  Threedim::InjectTexture n; // no scene, no handle, no name
  n();
  CHECK(n.outputs.scene_out.scene.state == nullptr);
  CHECK(n.outputs.scene_out.dirty == 0xFF); // first tick may announce itself

  n();
  CHECK(n.outputs.scene_out.scene.state == nullptr);
  CHECK(n.outputs.scene_out.dirty == 0); // today: 0xFF again, every tick
}

// =============================================================== ExtractTexture

TEST_CASE(
    "ExtractTexture resolves the named aux entry and stamps its metadata",
    "[threedim][extracttexture]")
{
  FakeRhiTexture decoy{QRhiTexture::BGRA8, QSize(4, 4), 0, 0, {}};
  FakeRhiTexture sky{QRhiTexture::RGBA32F, QSize(320, 240), 0, 0, {}};
  int decoy_sampler{}, sky_sampler{};

  Threedim::ExtractTexture n; // name defaults to "skybox"
  n.inputs.geometry.mesh.auxiliary_textures = {
      {.name = "decoy", .handle = &decoy, .sampler_handle = &decoy_sampler},
      {.name = "skybox", .handle = &sky, .sampler_handle = &sky_sampler},
  };
  run_extract(n);

  const auto& out = n.outputs.texture.texture;
  CHECK(out.handle == &sky);
  CHECK(out.sampler_handle == &sky_sampler); // producer sampler rides along
  CHECK(out.width == 320);
  CHECK(out.height == 240);
  CHECK(out.kind == halp::texture_kind::texture_2d);
  CHECK(out.layers_or_depth == 1);
  CHECK(out.format == halp::gpu_texture::RGBA32F);
}

TEST_CASE(
    "ExtractTexture shape detection: cubemap priority, 3D/array clamps",
    "[threedim][extracttexture]")
{
  auto extract = [](FakeRhiTexture& t) {
    Threedim::ExtractTexture n;
    n.inputs.geometry.mesh.auxiliary_textures.push_back(
        {.name = "skybox", .handle = &t, .sampler_handle = nullptr});
    run_extract(n);
    return n.outputs.texture.texture;
  };

  SECTION("cubemap is 6 layers regardless of arraySize")
  {
    FakeRhiTexture t{QRhiTexture::RGBA8, QSize(64, 64), 0, 12, QRhiTexture::CubeMap};
    const auto out = extract(t);
    CHECK(out.kind == halp::texture_kind::cubemap);
    CHECK(out.layers_or_depth == 6);
  }
  SECTION("cubemap wins when a backend sets both cube and 3D bits")
  {
    FakeRhiTexture t{
        QRhiTexture::RGBA8, QSize(64, 64), 32, 0,
        QRhiTexture::CubeMap | QRhiTexture::ThreeDimensional};
    CHECK(extract(t).kind == halp::texture_kind::cubemap);
  }
  SECTION("3D texture reports its depth, clamped up from 0")
  {
    FakeRhiTexture filled{
        QRhiTexture::RGBA8, QSize(8, 8), 12, 0, QRhiTexture::ThreeDimensional};
    const auto out = extract(filled);
    CHECK(out.kind == halp::texture_kind::texture_3d);
    CHECK(out.layers_or_depth == 12);

    FakeRhiTexture empty{
        QRhiTexture::RGBA8, QSize(8, 8), 0, 0, QRhiTexture::ThreeDimensional};
    CHECK(extract(empty).layers_or_depth == 1); // never a 0-depth binding
  }
  SECTION("texture array reports arraySize, clamped up from 0")
  {
    FakeRhiTexture filled{
        QRhiTexture::RGBA8, QSize(8, 8), 0, 8, QRhiTexture::TextureArray};
    const auto out = extract(filled);
    CHECK(out.kind == halp::texture_kind::texture_array);
    CHECK(out.layers_or_depth == 8);

    FakeRhiTexture empty{
        QRhiTexture::RGBA8, QSize(8, 8), 0, 0, QRhiTexture::TextureArray};
    CHECK(extract(empty).layers_or_depth == 1);
  }
  SECTION("format passthrough and the unmapped-format fallback")
  {
    // Qt-version-unguarded formats map one-to-one...
    FakeRhiTexture bgra{QRhiTexture::BGRA8, QSize(2, 2), 0, 0, {}};
    CHECK(extract(bgra).format == halp::gpu_texture::BGRA8);
    FakeRhiTexture r16f{QRhiTexture::R16F, QSize(2, 2), 0, 0, {}};
    CHECK(extract(r16f).format == halp::gpu_texture::R16F);
    // ...and a depth format (outside halp's enum) falls back to RGBA8 so the
    // downstream sampler binding does not trip a type mismatch.
    FakeRhiTexture d24{QRhiTexture::D24, QSize(2, 2), 0, 0, {}};
    CHECK(extract(d24).format == halp::gpu_texture::RGBA8);
  }
}

TEST_CASE(
    "ExtractTexture: a missing name degrades to the empty placeholder",
    "[threedim][extracttexture]")
{
  FakeRhiTexture sky{QRhiTexture::RGBA32F, QSize(320, 240), 0, 0, {}};

  Threedim::ExtractTexture n;
  n.inputs.geometry.mesh.auxiliary_textures.push_back(
      {.name = "skybox", .handle = &sky, .sampler_handle = nullptr});
  run_extract(n);
  REQUIRE(n.outputs.texture.texture.handle == &sky);

  // Retarget to a name nothing produces: everything the binding reads resets.
  n.inputs.name.value = "no_such_aux";
  run_extract(n);
  const auto& out = n.outputs.texture.texture;
  CHECK(out.handle == nullptr);
  CHECK(out.sampler_handle == nullptr);
  CHECK(out.width == 0);
  CHECK(out.height == 0);
  CHECK(out.layers_or_depth == 1);
  CHECK(out.kind == halp::texture_kind::texture_2d);
}

TEST_CASE(
    "ExtractTexture does not republish on an unchanged tick, and the first "
    "matching entry wins",
    "[threedim][extracttexture]")
{
  FakeRhiTexture first{QRhiTexture::RGBA8, QSize(16, 16), 0, 0, {}};
  int sampler_first{}, sampler_second{};

  Threedim::ExtractTexture n;
  n.inputs.geometry.mesh.auxiliary_textures = {
      {.name = "skybox", .handle = &first, .sampler_handle = &sampler_first},
      {.name = "skybox", .handle = &first, .sampler_handle = &sampler_second},
  };
  run_extract(n);
  CHECK(n.outputs.texture.texture.handle == &first);
  // Deterministic resolution: the first entry in declaration order.
  CHECK(n.outputs.texture.texture.sampler_handle == &sampler_first);

  // Metadata re-emission trips downstream SRB rebuilds, so an identical
  // (handle, name) tick must not touch the outlet at all. Scribble a sentinel
  // and prove the short-circuit leaves it alone.
  n.outputs.texture.texture.width = -777;
  run_extract(n);
  CHECK(n.outputs.texture.texture.width == -777);

  // A real retarget does republish (and overwrites the sentinel).
  FakeRhiTexture other{QRhiTexture::RGBA8, QSize(4, 4), 0, 0, {}};
  n.inputs.geometry.mesh.auxiliary_textures.push_back(
      {.name = "other", .handle = &other, .sampler_handle = nullptr});
  n.inputs.name.value = "other";
  run_extract(n);
  CHECK(n.outputs.texture.texture.handle == &other);
  CHECK(n.outputs.texture.texture.width == 4);
}

TEST_CASE(
    "ExtractTexture::release resets the outlet and forgets the cached state",
    "[threedim][extracttexture]")
{
  FakeRhiTexture sky{QRhiTexture::RGBA8, QSize(16, 16), 0, 0, {}};

  Threedim::ExtractTexture n;
  n.inputs.geometry.mesh.auxiliary_textures.push_back(
      {.name = "skybox", .handle = &sky, .sampler_handle = nullptr});
  run_extract(n);
  REQUIRE(n.outputs.texture.texture.handle == &sky);

  // release() only assigns members; its RenderList& is never dereferenced.
  n.release(inert_ref<score::gfx::RenderList>());
  CHECK(n.outputs.texture.texture.handle == nullptr);
  CHECK(n.outputs.texture.texture.width == 0);
  CHECK(n.outputs.texture.texture.height == 0);
  CHECK(n.outputs.texture.texture.layers_or_depth == 1);
  CHECK(n.outputs.texture.texture.kind == halp::texture_kind::texture_2d);

  // The last-known (handle, name) cache is cleared too: the next update with
  // unchanged inputs re-publishes rather than short-circuiting on stale state.
  run_extract(n);
  CHECK(n.outputs.texture.texture.handle == &sky);
  CHECK(n.outputs.texture.texture.width == 16);
}

// DEFECT: release() resets handle/width/height/layers/kind but NOT
// sampler_handle — the producer-owned sampler pointer forwarded by update()
// survives teardown on the output port. Same stale-resource class as the
// Camera::release m_state fix (tests/threedim/CameraRelease.cpp): after the
// render list is torn down that sampler is freed, and the outlet dangles.
TEST_CASE(
    "ExtractTexture::release clears the forwarded sampler handle",
    "[threedim][extracttexture][!shouldfail]")
{
  FakeRhiTexture sky{QRhiTexture::RGBA8, QSize(16, 16), 0, 0, {}};
  int sampler{};

  Threedim::ExtractTexture n;
  n.inputs.geometry.mesh.auxiliary_textures.push_back(
      {.name = "skybox", .handle = &sky, .sampler_handle = &sampler});
  run_extract(n);
  REQUIRE(n.outputs.texture.texture.sampler_handle == &sampler);

  n.release(inert_ref<score::gfx::RenderList>());
  CHECK(n.outputs.texture.texture.handle == nullptr);
  CHECK(n.outputs.texture.texture.sampler_handle == nullptr); // today: dangles
}

// ============================================================== TextureToBuffer

TEST_CASE(
    "TextureToBuffer::init clears the outlet and stops at the CPU guards",
    "[threedim][texturetobuffer]")
{
  Threedim::TextureToBuffer n;
  // custom_texture_base carries NO field initializers; set every field.
  n.inputs.texture.texture.bytes = nullptr;
  n.inputs.texture.texture.width = 0;
  n.inputs.texture.texture.height = 0;
  n.inputs.texture.texture.changed = false;
  n.inputs.texture.texture.format = halp::custom_texture_base::RGBA8;

  // Scribble the outlet to prove init() zeroes it before any early-out.
  int garbage{};
  n.outputs.buffer.buffer.handle = &garbage;
  n.outputs.buffer.buffer.byte_size = 123;
  n.outputs.buffer.buffer.byte_offset = 7;
  n.outputs.buffer.buffer.changed = false;

  SECTION("no CPU bytes at all") { } // bytes == nullptr
  SECTION("bytes present but a zero-sized description")
  {
    static unsigned char pixel[4]{};
    n.inputs.texture.texture.bytes = pixel; // 0x0 → bytesize() == 0
  }

  // Both guards return before the first `renderer.` use (buf is null, so the
  // releaseBuffer branch is not taken either): the inert reference is safe.
  n.init(inert_ref<score::gfx::RenderList>(), inert_ref<QRhiResourceUpdateBatch>());
  CHECK(n.outputs.buffer.buffer.handle == nullptr);
  CHECK(n.outputs.buffer.buffer.byte_size == 0);
  CHECK(n.outputs.buffer.buffer.byte_offset == 0);
  CHECK(n.outputs.buffer.buffer.changed == true);
  CHECK(n.buf == nullptr);
}

TEST_CASE(
    "TextureToBuffer::update re-inits only on a byte-size mismatch",
    "[threedim][texturetobuffer]")
{
  // Paper-computed sizes for the formats the node compares against
  // QRhiBuffer::size(): bytesize() = component_size * components * w * h.
  Threedim::TextureToBuffer n;
  static unsigned char data[4 * 4 * 16]{}; // large enough for every case below
  n.inputs.texture.texture.bytes = data;
  n.inputs.texture.texture.width = 4;
  n.inputs.texture.texture.height = 4;
  n.inputs.texture.texture.changed = true;
  n.inputs.texture.texture.format = halp::custom_texture_base::RGBA8;
  CHECK(n.inputs.texture.texture.bytesize() == 4 * 4 * 4); // 1B x 4 comp x 16 px

  n.inputs.texture.texture.format = halp::custom_texture_base::RGBA32F;
  CHECK(n.inputs.texture.texture.bytesize() == 4 * 4 * 16); // 4B x 4 comp

  n.inputs.texture.texture.format = halp::custom_texture_base::R16F;
  n.inputs.texture.texture.width = 7;
  n.inputs.texture.texture.height = 3;
  CHECK(n.inputs.texture.texture.bytesize() == 7 * 3 * 2); // 2B x 1 comp

  SECTION("matching size: no re-init, the published outlet is left alone")
  {
    n.inputs.texture.texture.format = halp::custom_texture_base::RGBA8;
    n.inputs.texture.texture.width = 4;
    n.inputs.texture.texture.height = 4;

    FakeRhiBuffer buf{64}; // == bytesize(); only its inline size() is read
    n.buf = &buf;
    n.outputs.buffer.buffer.handle = &buf;
    n.outputs.buffer.buffer.byte_size = 123; // sentinel: must survive
    n.update(
        inert_ref<score::gfx::RenderList>(), inert_ref<QRhiResourceUpdateBatch>(),
        nullptr);
    CHECK(n.outputs.buffer.buffer.byte_size == 123);
    CHECK(n.outputs.buffer.buffer.handle == &buf);
    n.buf = nullptr; // the stack stub must not outlive the node's view of it
  }

  SECTION("no buffer yet + no CPU bytes: update re-inits through the guard")
  {
    n.inputs.texture.texture.bytes = nullptr;
    n.outputs.buffer.buffer.byte_size = 123;
    n.update(
        inert_ref<score::gfx::RenderList>(), inert_ref<QRhiResourceUpdateBatch>(),
        nullptr);
    CHECK(n.outputs.buffer.buffer.byte_size == 0); // init() ran and zeroed
    CHECK(n.outputs.buffer.buffer.changed == true);
  }

  // The mismatch-with-live-buffer path (releaseBuffer + newBuffer + create)
  // dereferences the RenderList and is out of scope here — GPU work.
}

// DEFECT: a texture dropout followed by data returning at the SAME size leaves
// the outlet permanently null. init()'s `!bytes` early-out zeroes the published
// outputs but keeps `buf`, and update()'s only republish trigger is a byte-size
// mismatch — so once sizes match again nothing ever re-publishes the handle,
// while runInitialPasses() happily uploads into the live buffer downstream can
// no longer see. Production sequence: (1) texture 4x4 → init allocates+publishes,
// (2) source drops out (bytes=null, dims 0) → update re-inits, zeroes the outlet,
// returns before releasing buf, (3) source returns at 4x4 → sizes match, no init,
// outlet stays null. Steps 2-3 are driven verbatim below; the stub buffer stands
// in for step 1's GPU allocation (same size, only its inline size() is read).
TEST_CASE(
    "TextureToBuffer republishes the buffer when data returns at the same size",
    "[threedim][texturetobuffer][!shouldfail]")
{
  Threedim::TextureToBuffer n;
  FakeRhiBuffer buf{64}; // step 1's allocation: 4x4 RGBA8
  n.buf = &buf;
  n.outputs.buffer.buffer.handle = &buf;
  n.outputs.buffer.buffer.byte_size = 64;

  // Step 2: dropout tick.
  n.inputs.texture.texture.bytes = nullptr;
  n.inputs.texture.texture.width = 0;
  n.inputs.texture.texture.height = 0;
  n.inputs.texture.texture.changed = false;
  n.inputs.texture.texture.format = halp::custom_texture_base::RGBA8;
  n.update(
      inert_ref<score::gfx::RenderList>(), inert_ref<QRhiResourceUpdateBatch>(),
      nullptr);
  CHECK(n.outputs.buffer.buffer.handle == nullptr); // outlet cleared...
  CHECK(n.buf == &buf);                             // ...but the buffer is kept

  // Step 3: the source comes back at the same 4x4 RGBA8 = 64 bytes.
  static unsigned char data[64]{};
  n.inputs.texture.texture.bytes = data;
  n.inputs.texture.texture.width = 4;
  n.inputs.texture.texture.height = 4;
  n.inputs.texture.texture.changed = true;
  n.update(
      inert_ref<score::gfx::RenderList>(), inert_ref<QRhiResourceUpdateBatch>(),
      nullptr);

  // Correct behaviour: downstream gets the (still-live, size-matching) buffer
  // back. Today the outlet stays null forever.
  CHECK(n.outputs.buffer.buffer.handle == &buf);
  CHECK(n.outputs.buffer.buffer.byte_size == 64);

  n.buf = nullptr; // stack stub hygiene
}
