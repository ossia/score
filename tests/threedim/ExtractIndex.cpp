// L3 regression guard — split/threedim finding #7 (ExtractBuffer2 index-buffer
// sizing).
//
// ExtractBuffer2::resolveBuffer()'s name=="index" branch sized the index
// buffer as mesh.vertices * elemsize. The index-element count is mesh.indices,
// a field entirely distinct from mesh.vertices, so the published byte_size was
// wrong: it under-reports (dropping triangles) or over-reports (downstream SSBO
// reads past the end). The fix uses mesh.indices, and returns an empty ref when
// mesh.indices <= 0 (non-indexed / unpopulated) so the outlet clears.
//
// resolveBuffer is a pure, static logic function over halp::dynamic_gpu_geometry
// (no GPU), but it is private. We expose it for the test via `#define private
// public` around the header include — the compiled ExtractBuffer2.cpp provides
// the real definition, so we are exercising shipped engine code. Reverting the
// fix restores mesh.vertices sizing and drops the indices<=0 guard (RED).

// Pre-include everything ExtractBuffer2.hpp (and Catch2) pull in, WITHOUT the
// access-override macro, so `#define private public` only affects
// ExtractBuffer2's own class body — not std / Qt headers (e.g. <any>, which
// redeclares members with explicit access and breaks under the macro).
#include <catch2/catch_test_macros.hpp>

#include <Threedim/GeometryToBufferStrategies.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

#define private public
#include <Threedim/ExtractBuffer2.hpp>
#undef private

namespace
{
halp::dynamic_gpu_geometry
makeIndexedMesh(int vertices, int indices, halp::index_format fmt)
{
  static int dummy = 0;
  halp::dynamic_gpu_geometry mesh;
  halp::geometry_gpu_buffer buf;
  buf.handle = &dummy;
  buf.byte_size = 1 << 20; // large backing buffer; irrelevant to index sizing
  mesh.buffers.push_back(buf);
  mesh.index.buffer = 0;
  mesh.index.byte_offset = 0;
  mesh.index.format = fmt;
  mesh.vertices = vertices;
  mesh.indices = indices;
  return mesh;
}
} // namespace

TEST_CASE(
    "ExtractBuffer2 index buffer is sized by index count (uint32)",
    "[threedim][extractbuffer][f7]")
{
  // Cube: 8 vertices, 36 uint32 indices. Wrong (pre-fix): 8*4 = 32. Right: 144.
  auto mesh = makeIndexedMesh(8, 36, halp::index_format::uint32);
  auto ref = Threedim::ExtractBuffer2::resolveBuffer(mesh, "index");
  CHECK(ref.buffer_index == 0);
  CHECK(ref.byte_size == 36 * 4);
}

TEST_CASE(
    "ExtractBuffer2 index buffer is sized by index count (uint16)",
    "[threedim][extractbuffer][f7]")
{
  // vertex_count=1000 but index_count=60, uint16. Wrong (pre-fix): 2000. Right:
  // 120. (Over-report would read 1880 bytes past the real index buffer.)
  auto mesh = makeIndexedMesh(1000, 60, halp::index_format::uint16);
  auto ref = Threedim::ExtractBuffer2::resolveBuffer(mesh, "index");
  CHECK(ref.buffer_index == 0);
  CHECK(ref.byte_size == 60 * 2);
}

TEST_CASE(
    "ExtractBuffer2 index buffer with zero indices clears the outlet",
    "[threedim][extractbuffer][f7]")
{
  // Non-indexed / unpopulated: indices==0 must yield an empty ref (cleared
  // outlet), not a vertices-sized garbage range.
  auto mesh = makeIndexedMesh(8, 0, halp::index_format::uint32);
  auto ref = Threedim::ExtractBuffer2::resolveBuffer(mesh, "index");
  CHECK(ref.buffer_index == -1);
  CHECK(ref.byte_size == 0);
}
