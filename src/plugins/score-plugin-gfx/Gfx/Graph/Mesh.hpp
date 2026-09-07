#pragma once

#include <cstdint>
#include <limits>
#include <Process/ProcessFlags.hpp>

#include <ossia/dataflow/geometry_port.hpp>
#include <ossia/detail/small_vector.hpp>
#include <span>

#include <private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

namespace score::gfx
{
struct BufferView
{
  QRhiBuffer* handle{};
  int64_t byte_offset{};
  int64_t byte_size{};

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  enum class Usage : uint8_t
  {
    Direct,
    IndirectDraw,
    IndirectDrawIndexed
  };
  Usage usage{Usage::Direct};
#endif

  // False for borrowed buffers — e.g., gpu_buffer handles the caller
  // owns (scene preprocessor's MDI arena buffers, registry arena
  // buffers). RenderList::release only `delete`s when owned=true; owners
  // outside the RenderList's m_vertexBuffers destroy their own handles.
  bool owned{true};

  inline operator bool() const noexcept { return handle; }
};
// A QRhiBuffer's size is a quint32 (qrhi.h: `quint32 size() const`,
// `setSize(quint32)`), while score carries geometry sizes as int64_t. Narrowing
// is SILENT and catastrophic: 4 GiB + 4 KiB becomes 4 KiB, the allocation
// succeeds, and every upload and draw afterwards addresses it as though it held
// the original size.
//
// So ask before narrowing. A size that does not survive the round trip is not
// expressible and the caller must refuse it rather than hand QRhi a number that
// means something else.
inline bool bufferSizeIsExpressible(int64_t byte_size) noexcept
{
  return byte_size >= 0 && byte_size <= int64_t(std::numeric_limits<quint32>::max());
}

// A draw command whose index/vertex count or instance count is zero paints
// nothing. Every backend agrees on the outcome -- but they do not agree on how
// to get there. Vulkan, D3D and GL treat it as a legal no-op; Metal's API
// validation layer treats it as a programming error and ABORTS the process
// ("indexCount(0) must be non-zero" / "instanceCount(0) must be non-zero"),
// taking the whole app down.
//
// That matters because zero-count commands are not a pathology here, they are
// the CONTRACT. RenderState::Caps requires indirect producers to keep the
// command slots beyond their written count ZEROED, which is what makes the
// full-capacity multi-draw rung (R2) equivalent to the GPU-count rung (R1).
// When a producer publishes no "_indirect_draw_count" auxiliary there is no
// count to clamp with, so the CPU-readback rung (R4) replays every capacity
// slot verbatim -- dead zeroed ones included -- as explicit draw calls. On the
// indirect rungs the GPU discards them silently; on the CPU rung they become
// exactly the API call Metal refuses.
//
// So the rungs are only equivalent if the CPU rung skips what the GPU rung
// would have discarded. Filter here.
inline bool drawCommandPaints(const ossia::geometry::draw_command& cmd) noexcept
{
  return cmd.index_or_vertex_count > 0 && cmd.instance_count > 0;
}

// Observability for the filter above. Without it the filter is invisible on
// every backend that is not Metal: the pixels are identical whether the dead
// slots were skipped or issued as no-op draws, so a test on Linux could not
// tell a working filter from a deleted one.
//
// Two mechanisms, because they answer different questions. The log line is for
// a human reading a session's output and is warn-once, so it costs nothing in
// a real render loop. The counter is for tests: warn-once is useless as an
// assertion when a Catch2 process runs several backends in sequence -- the
// first session consumes the only line and every later one sees nothing. A
// test brackets a session and asserts the delta.
SCORE_PLUGIN_GFX_EXPORT void noteZeroCountSlotsSkipped(int skipped) noexcept;
SCORE_PLUGIN_GFX_EXPORT uint64_t zeroCountSlotsSkippedTotal() noexcept;

struct MeshBuffers
{
  ossia::small_vector<BufferView, 2> buffers;

  // --- Multi-draw indirect state ---
  // Always tracked regardless of Qt version. At draw time the path is:
  //   gpuIndirectSupported && indirectDrawBuffer → drawIndirect (GPU, Qt 6.12+)
  //   !gpuIndirectSupported && cpuDrawCommands   → per-command drawIndexed loop
  //   neither                                    → single drawIndexed
  QRhiBuffer* indirectDrawBuffer{};
  bool useIndirectDraw{false};
  bool indirectDrawIndexed{false};
  bool gpuIndirectSupported{false};  // set from RenderState::caps at init
  quint32 indirectDrawOffset{0};
  quint32 indirectDrawCount{1};
  quint32 indirectDrawStride{0};

  // The uniform record the engine emits for one indirect command: 5 u32.
  // Both layouts share it -- the non-indexed one is a native 4-word
  // QRhiDrawIndirectCommand plus a trailing word, the indexed one is a native
  // 5-word QRhiDrawIndexedIndirectCommand. See the libisf codegen.
  static constexpr quint32 indirect_record_stride = 5 * sizeof(uint32_t);

  // Turn the indirect path on. Use this rather than assigning the fields:
  // setting useIndirectDraw without a stride is not a degradation, it is an
  // ABORT -- indirectDrawStride defaults to 0 and QRhi asserts
  // `stride >= sizeof(QRhi[Indexed]IndirectDrawCommand)` inside drawIndirect /
  // drawIndexedIndirect, so the process dies the first time that mesh draws.
  // The flag and the stride are not independently meaningful, so they are not
  // independently settable.
  void enableIndirectDraw(
      QRhiBuffer* buffer, bool indexed, int64_t byte_size,
      quint32 byte_offset = 0) noexcept
  {
    indirectDrawBuffer = buffer;
    useIndirectDraw = true;
    indirectDrawIndexed = indexed;
    indirectDrawOffset = byte_offset;
    indirectDrawStride = indirect_record_stride;
    indirectDrawCount = quint32(byte_size / indirect_record_stride);
    if(indirectDrawCount == 0)
      indirectDrawCount = 1;
  }

  // --- GPU-decided draw count (Qt 6.13-era drawIndexedIndirectCount) ---
  // When indirectCountBuffer is set AND gpuIndirectCountSupported, the draw
  // reads its command count as a u32 out of this buffer at execution time,
  // clamped to indirectDrawCount (which then acts as maxDrawCount == the
  // command-slot capacity). Producers publish the buffer through the
  // "_indirect_draw_count" auxiliary convention (CustomMesh picks it up).
  //
  // When the count rung is unavailable, correctness relies on the producer
  // contract stated in RenderState::Caps: command slots beyond the written
  // count stay zeroed, so the full-capacity multi-draw rung paints the same
  // pixels. The CPU-readback fallback reads this buffer too and clamps
  // cpuDrawCommands to the GPU-written count.
  QRhiBuffer* indirectCountBuffer{};
  quint32 indirectCountOffset{0};
  bool gpuIndirectCountSupported{false}; // caps.drawIndirectCount &&
                                         // drawIndirectCountUsable()
  bool gpuIndirectMultiSupported{false}; // caps.drawIndirectMulti; false =>
                                         // per-command drawCount=1 loop rung

  // CPU-side draw commands. Populated either:
  //   a) directly by the producer (ScenePreprocessor has CPU data), or
  //   b) via GPU readback when the indirect buffer is GPU-generated (CSF)
  //      and gpuIndirectSupported is false.
  ossia::small_vector<ossia::geometry::draw_command, 0> cpuDrawCommands;

  // Readback result storage for the synchronous GPU→CPU fallback in
  // RenderedRawRasterPipelineNode::runInitialPasses.
  // Qt < 6.6 has a separate type for buffer readbacks.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  QRhiReadbackResult readbackResult;
  // Second slot for the 4-byte GPU-written draw count, read back alongside
  // the commands so the CPU loop can clamp to it (see indirectCountBuffer).
  QRhiReadbackResult countReadbackResult;
#else
  QRhiBufferReadbackResult readbackResult;
  QRhiBufferReadbackResult countReadbackResult;
#endif
};
/**
 * @brief Data model for meshes.
 */
struct SCORE_PLUGIN_GFX_EXPORT Mesh
{
public:
  explicit Mesh();
  virtual ~Mesh();

  enum Flag
  {
    HasPosition = SCORE_FLAG(1),
    HasTexCoord = SCORE_FLAG(2),
    HasColor = SCORE_FLAG(3),
    HasNormals = SCORE_FLAG(4),
    HasTangents = SCORE_FLAG(5),
  };
  using Flags = QFlags<Flag>;

  [[nodiscard]] virtual Flags flags() const noexcept = 0;

  [[nodiscard]] virtual MeshBuffers init(QRhi& rhi) const noexcept = 0;

  virtual void
  update(QRhi& rhi, MeshBuffers& bufs, QRhiResourceUpdateBatch& cb) const noexcept
      = 0;
  virtual void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept = 0;

  // False when the mesh currently carries no sub-mesh: its vertex-input
  // layout is empty and cannot satisfy a vertex shader that declares inputs.
  [[nodiscard]] virtual bool hasGeometry() const noexcept { return true; }
  virtual void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept = 0;

  /** @brief A basic vertex shader that is going to work with this mesh. */
  virtual const char* defaultVertexShader() const noexcept = 0;

  /** @brief Return the underlying semantic geometry if available.
   *
   * Used by buildPipeline() to remap vertex input layouts by matching
   * shader input variable names to geometry attribute semantics.
   * Meshes that carry semantic information (e.g. CustomMesh) should override this.
   */
  virtual const ossia::geometry* semanticGeometry() const noexcept { return nullptr; }

  ossia::geometry_filter_list_ptr filters;

  std::atomic_int64_t dirtyGeometryIndex{-1};

  bool hasGeometryChanged(int64_t& renderer) const noexcept
  {
    int64_t res = dirtyGeometryIndex.load(std::memory_order_acquire);
    if(renderer != res)
    {
      renderer = res;
      return true;
    }
    return false;
  }

protected:
  /*
*/
private:
  Mesh(const Mesh&) = delete;
  Mesh(Mesh&&) = delete;
  Mesh& operator=(const Mesh&) = delete;
  Mesh& operator=(Mesh&&) = delete;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Mesh::Flags);

struct SCORE_PLUGIN_GFX_EXPORT BasicMesh : Mesh
{
  using Mesh::Mesh;
  [[nodiscard]] virtual MeshBuffers init(QRhi& rhi) const noexcept override;
  void update(
      QRhi& rhi, MeshBuffers& bufs, QRhiResourceUpdateBatch& cb) const noexcept override;
  void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept override;
  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
  virtual void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept = 0;

  using pip = QRhiGraphicsPipeline;
  pip::Topology topology = pip::Topology::TriangleStrip;
  pip::CullMode cullMode = pip::CullMode::None;
  pip::FrontFace frontFace = pip::FrontFace::CW;

  ossia::small_vector<QRhiVertexInputBinding, 2> vertexBindings;
  ossia::small_vector<QRhiVertexInputAttribute, 2> vertexAttributes;

  std::span<const float> vertexArray;
  int vertexCount{};
};
/*
* @brief A dummy mesh for only accessing a gl_VertexId without caring about attributes
*/
struct SCORE_PLUGIN_GFX_EXPORT DummyMesh : BasicMesh
{
  explicit DummyMesh(int count);
  [[nodiscard]] Flags flags() const noexcept override { return Flags{}; }
  const char* defaultVertexShader() const noexcept override;
  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

/**
 * @brief A mesh with only position attributes.
 */
struct SCORE_PLUGIN_GFX_EXPORT PlainMesh : BasicMesh
{
  explicit PlainMesh(std::span<const float> vtx, int count);
  [[nodiscard]] Flags flags() const noexcept override { return HasPosition; }
  const char* defaultVertexShader() const noexcept override;
  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

/**
 * @brief A mesh with positions and texture coordinates.
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedMesh : BasicMesh
{
  explicit TexturedMesh(std::span<const float> vtx, int count);
  [[nodiscard]] Flags flags() const noexcept override
  {
    return HasPosition | HasTexCoord;
  }

  const char* defaultVertexShader() const noexcept override;
};

/**
 * @brief A triangle mesh with only positions.
 */
struct SCORE_PLUGIN_GFX_EXPORT PlainTriangle final : PlainMesh
{
  static const constexpr float data[] = {-1, -1, 3, -1, -1, 3};

  explicit PlainTriangle();
  static const PlainTriangle& instance() noexcept;
};

/**
 * @brief A triangle mesh with positions and texture coordinates.
 *
 * This is the main mesh being used for rendering full-screen effects.
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedTriangle final : TexturedMesh
{
  static const constexpr float data[] = {// positions
                                         -1, -1, 3, -1, -1, 3,
                                         // tex coords
                                         0, 0, 2, 0, 0, 2};
  static const constexpr float flipped_y_data[] = {// positions
                                                   -1, -1, 3, -1, -1, 3,
                                                   // tex coords
                                                   0, 2, 2, 2, 0, 0};

  explicit TexturedTriangle(bool flipped = false);

  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

/**
 * @brief A quad mesh with positions and texture coordinates.
 *
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedQuad final : TexturedMesh
{
  static const constexpr float data[] = {// positions
                                         -1, -1, +1, -1, -1, +1, +1, +1,
                                         // tex coords
                                         0, 0, 1, 0, 0, 1, 1, 1};

  static const constexpr float flipped_y_data[] = {// positions
                                                   -1, -1, +1, -1, -1, +1, +1, +1,
                                                   // tex coords
                                                   0, 1, 1, 1, 0, 0, 1, 0};

  explicit TexturedQuad(bool flipped = false);

  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

/**
 * @brief Draw a mesh, using indirect multi-draw when available in MeshBuffers.
 *
 * When `bufs.useIndirectDraw` is true (and Qt >= 6.12), dispatches to
 * `cb.drawIndexedIndirect` / `cb.drawIndirect` with the offset/count/stride
 * stored in `bufs`. Otherwise falls back to the mesh's standard `draw()`.
 *
 * This is the main draw entry point for ISF / RawRaster / Scene renderers so
 * that they can transparently support multi-draw indirect just by wiring an
 * indirect buffer into MeshBuffers.
 */
SCORE_PLUGIN_GFX_EXPORT
void drawMeshWithOptionalIndirect(
    const Mesh& mesh, const MeshBuffers& bufs, QRhiCommandBuffer& cb) noexcept;

}
