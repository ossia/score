// SplatLoader (BufferLoader.{hpp,cpp} — the "Splat loader" node): CPU-side
// contract of the node around GaussianSplatsFromPly. The PLY *parsing* itself
// is covered in GeometryLoaderFormats.cpp; this file covers the node seam:
//
//   - ins::obj_t::process(file_type) loads the file SYNCHRONOUSLY (inside
//     process(), i.e. on the avendish worker thread) and returns a closure
//     that is later applied on the processing thread. We drive that exact
//     two-step protocol, as VoxelAssets.cpp does for VoxelLoader.
//   - Applying the closure must publish the loaded 64-float-per-splat records
//     byte-exactly into m_splat_data (buffer / splatCount / shRestCount) and
//     raise m_changed so the next GPU update() re-uploads.
//   - A missing/unparseable file degrades to an empty load (splatCount == 0,
//     empty buffer) without crashing; update()'s splatCount guard then makes
//     the raised m_changed a no-op.
//   - A new path reloads and re-dirties; idle ticks (operator(), a no-op by
//     design) must NOT re-raise m_changed once the renderer consumed it —
//     pinning the spurious-re-dirty defect class (cf. the TagAs fix) as
//     currently-correct.
//
// Out of scope, honestly: init()/update()/release() need a live
// score::gfx::RenderList with a real QRhi (state.rhi->newBuffer,
// releaseBuffer) — there is no headless seam before the first dereference, so
// the upload path is left to the gfx/integration tier. All expected buffer
// contents below are binary-exact float literals, so comparisons use == and
// the whole 256-byte record is checked, not just spot fields. App-free, no
// GPU, deterministic; inputs are generated in a QTemporaryDir.

#include <Threedim/BufferLoader.hpp>
#include <Threedim/Ply.hpp>

#include <QTemporaryDir>

#include <halp/file_port.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <fstream>
#include <string>

namespace
{

// ---------------------------------------------------------------- helpers

// Same shape as tests/unit/PrimitiveCloudTest.cpp's TempTree.
struct TempTree
{
  QTemporaryDir dir;

  std::string write(const char* name, const std::string& bytes)
  {
    REQUIRE(dir.isValid());
    const auto path = dir.filePath(QString::fromUtf8(name)).toStdString();
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    REQUIRE(f.good());
    f.write(bytes.data(), (std::streamsize)bytes.size());
    f.close();
    REQUIRE(f.good());
    return path;
  }
};

// Drives the file port exactly as the runtime does: process() on the (worker)
// call site, then the returned closure applied to the node instance.
void load(Threedim::SplatLoader& loader, std::string_view filename)
{
  halp::mmap_file_view tv;
  tv.filename = filename;
  tv.bytes = {}; // process() reads from disk by filename, not from bytes
  auto apply = Threedim::SplatLoader::ins::obj_t::process(tv);
  REQUIRE(bool(apply));
  apply(loader);
}

constexpr uint32_t kFloats = Threedim::GaussianSplatData::floatsPerSplat; // 64

// Classic 3DGS-export vertex schema (positions, normals, SH DC, opacity,
// log-scale, quaternion; no f_rest columns). All values are binary-exact.
const std::string classic_two_splat_ply = R"(ply
format ascii 1.0
element vertex 2
property float x
property float y
property float z
property float nx
property float ny
property float nz
property float f_dc_0
property float f_dc_1
property float f_dc_2
property float opacity
property float scale_0
property float scale_1
property float scale_2
property float rot_0
property float rot_1
property float rot_2
property float rot_3
end_header
1 2 3 0 0 1 0.5 0.25 -0.5 -1.5 -2 -3 -4 1 0 0 0
4 5 6 0 1 0 0.75 -0.25 0.125 2.5 -1 -2 -3 0 1 0 0
)";

// The 2 * 64-float records the documented layout maps the file above to
// (Ply.hpp: pos 0-2, normal 3-5, SH DC 6-8, f_rest 9-53, opacity 54,
// scale 55-57, rot 58-61, padding 62-63; everything absent zero-filled).
std::array<float, 2 * kFloats> expected_two_splats()
{
  std::array<float, 2 * kFloats> e{}; // zero-fill covers f_rest + padding
  float* s0 = e.data();
  s0[0] = 1.f; s0[1] = 2.f; s0[2] = 3.f;    // position
  s0[3] = 0.f; s0[4] = 0.f; s0[5] = 1.f;    // normal
  s0[6] = 0.5f; s0[7] = 0.25f; s0[8] = -0.5f; // SH DC
  s0[54] = -1.5f;                           // opacity (pre-sigmoid)
  s0[55] = -2.f; s0[56] = -3.f; s0[57] = -4.f; // scale (log-space)
  s0[58] = 1.f; s0[59] = 0.f; s0[60] = 0.f; s0[61] = 0.f; // rot wxyz

  float* s1 = e.data() + kFloats;
  s1[0] = 4.f; s1[1] = 5.f; s1[2] = 6.f;
  s1[3] = 0.f; s1[4] = 1.f; s1[5] = 0.f;
  s1[6] = 0.75f; s1[7] = -0.25f; s1[8] = 0.125f;
  s1[54] = 2.5f;
  s1[55] = -1.f; s1[56] = -2.f; s1[57] = -3.f;
  s1[58] = 0.f; s1[59] = 1.f; s1[60] = 0.f; s1[61] = 0.f;
  return e;
}

// Position-only splat cloud: everything else must come out zeroed.
std::string xyz_only_ply(std::string_view row)
{
  std::string s = "ply\nformat ascii 1.0\nelement vertex 1\n"
                  "property float x\nproperty float y\nproperty float z\n"
                  "end_header\n";
  s += row;
  s += "\n";
  return s;
}

} // namespace

// ------------------------------------------------------------- test cases

TEST_CASE(
    "SplatLoader file port loads a 3DGS PLY byte-exactly into node state",
    "[threedim][splat][bufferloader]")
{
  TempTree t;
  const auto path = t.write("two.ply", classic_two_splat_ply);

  Threedim::SplatLoader loader;

  // Pristine node: nothing loaded, nothing dirty, no GPU handle published.
  CHECK(loader.m_changed == false);
  CHECK(loader.m_splat_data.splatCount == 0);
  CHECK(loader.m_splat_data.buffer.empty());
  CHECK(loader.outputs.buffer.buffer.handle == nullptr);
  CHECK(loader.outputs.buffer.buffer.byte_size == 0);
  CHECK(loader.outputs.buffer.buffer.changed == false);

  load(loader, path);

  CHECK(loader.m_changed == true);
  CHECK(loader.m_splat_data.splatCount == 2);
  CHECK(loader.m_splat_data.shRestCount == 0);
  REQUIRE(loader.m_splat_data.buffer.size() == 2 * kFloats);

  // The byte size update() would hand to QRhi: 2 records * 256 bytes.
  CHECK(
      loader.m_splat_data.buffer.size() * sizeof(float)
      == 2 * Threedim::GaussianSplatData::bytesPerSplat);
  CHECK(Threedim::GaussianSplatData::bytesPerSplat == 256);

  // Full-record comparison — every one of the 128 floats, including the
  // zero-filled f_rest region and the 2-float tail padding of each record.
  const auto expected = expected_two_splats();
  for(uint32_t i = 0; i < 2 * kFloats; i++)
  {
    INFO("float index " << i << " (splat " << i / kFloats << ", field "
                        << i % kFloats << ")");
    CHECK(loader.m_splat_data.buffer[i] == expected[i]);
  }
}

TEST_CASE(
    "SplatLoader forwards the SH-rest coefficient count into node state",
    "[threedim][splat][bufferloader]")
{
  std::string header = "ply\nformat ascii 1.0\nelement vertex 1\n"
                       "property float x\nproperty float y\nproperty float z\n";
  std::string row = "1 2 3";
  for(int i = 0; i < 9; i++)
  {
    header += "property float f_rest_" + std::to_string(i) + "\n";
    row += " " + std::to_string(i) + ".5"; // 0.5, 1.5, ... 8.5: binary-exact
  }
  header += "end_header\n" + row + "\n";

  TempTree t;
  const auto path = t.write("sh9.ply", header);

  Threedim::SplatLoader loader;
  load(loader, path);

  CHECK(loader.m_splat_data.splatCount == 1);
  CHECK(loader.m_splat_data.shRestCount == 9);
  REQUIRE(loader.m_splat_data.buffer.size() == kFloats);
  for(int i = 0; i < 9; i++)
    CHECK(loader.m_splat_data.buffer[9 + i] == float(i) + 0.5f);
  for(int i = 9 + 9; i < 54; i++) // f_rest slots past the 9 present stay zero
    CHECK(loader.m_splat_data.buffer[i] == 0.f);
  CHECK(loader.m_changed == true);
}

TEST_CASE(
    "SplatLoader degrades a missing or unparseable file to an empty load",
    "[threedim][splat][bufferloader][malformed]")
{
  TempTree t;
  Threedim::SplatLoader loader;

  // Start from a good load so we can see the failure actually clearing it.
  load(loader, t.write("good.ply", classic_two_splat_ply));
  REQUIRE(loader.m_splat_data.splatCount == 2);
  loader.m_changed = false; // renderer consumed the first upload

  // Missing file: process() still yields a closure; applying it swaps in the
  // empty result (splatCount 0, empty buffer) and raises m_changed — which
  // update()'s `splatCount <= 0` guard then turns into a no-op upload.
  load(loader, "/nonexistent/score-threedim/no-such.ply");
  CHECK(loader.m_splat_data.splatCount == 0);
  CHECK(loader.m_splat_data.buffer.empty());
  CHECK(loader.m_changed == true);

  // Header-only junk parses to zero rows just the same, without crashing.
  loader.m_changed = false;
  load(loader, t.write("junk.ply", "ply\nformat ascii 1.0\nelement vertex 4\n"));
  CHECK(loader.m_splat_data.splatCount == 0);
  CHECK(loader.m_splat_data.buffer.empty());
}

TEST_CASE(
    "SplatLoader reloads on a new path and does not re-dirty on idle ticks",
    "[threedim][splat][bufferloader]")
{
  TempTree t;
  const auto pathA = t.write("a.ply", xyz_only_ply("7 8 9"));
  const auto pathB = t.write("b.ply", classic_two_splat_ply);

  Threedim::SplatLoader loader;

  load(loader, pathA);
  REQUIRE(loader.m_splat_data.splatCount == 1);
  CHECK(loader.m_splat_data.buffer[0] == 7.f);
  CHECK(loader.m_splat_data.buffer[1] == 8.f);
  CHECK(loader.m_splat_data.buffer[2] == 9.f);
  CHECK(loader.m_changed == true);

  // The renderer consumes the dirty flag (as update() does after uploading).
  loader.m_changed = false;

  // Spurious-re-dirty pin (the plugin defect class fixed in TagAs): ticking
  // the node with an UNCHANGED path must not raise m_changed again nor touch
  // the loaded data. SplatLoader's operator() is a deliberate no-op — pin it.
  for(int i = 0; i < 3; i++)
    loader();
  CHECK(loader.m_changed == false);
  CHECK(loader.m_splat_data.splatCount == 1);
  REQUIRE(loader.m_splat_data.buffer.size() == kFloats);
  CHECK(loader.m_splat_data.buffer[0] == 7.f);

  // Path control changes -> the runtime calls process() with the new file;
  // applying its closure swaps in the new data and re-dirties.
  load(loader, pathB);
  CHECK(loader.m_changed == true);
  CHECK(loader.m_splat_data.splatCount == 2);
  REQUIRE(loader.m_splat_data.buffer.size() == 2 * kFloats);
  CHECK(loader.m_splat_data.buffer[0] == 1.f);
  CHECK(loader.m_splat_data.buffer[kFloats + 0] == 4.f);
}
