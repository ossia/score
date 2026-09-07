#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/RhiIndirectCompat.hpp>
#include <score/tools/Debug.hpp>

#include <QDebug>

#include <atomic>

namespace score::gfx
{
static std::atomic<uint64_t> g_zeroCountSlotsSkipped{0};

void noteZeroCountSlotsSkipped(int skipped) noexcept
{
  if(skipped <= 0)
    return;
  g_zeroCountSlotsSkipped.fetch_add((uint64_t)skipped, std::memory_order_relaxed);
  static bool warned = false;
  if(warned)
    return;
  warned = true;
  qDebug() << "score.gfx: indirect CPU rung: skipped" << skipped
           << "zero-count command slot(s)";
}

uint64_t zeroCountSlotsSkippedTotal() noexcept
{
  return g_zeroCountSlotsSkipped.load(std::memory_order_relaxed);
}


Mesh::Mesh() = default;

Mesh::~Mesh() = default;

MeshBuffers BasicMesh::init(QRhi& rhi) const noexcept
{
  auto mesh_buf = rhi.newBuffer(
      QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
      vertexArray.size() * sizeof(float));
  mesh_buf->setName("BasicMesh::mesh_buf");
  mesh_buf->create();

  MeshBuffers ret{{BufferView{mesh_buf, 0, 0}}};
  return ret;
}

void BasicMesh::update(
    QRhi& rhi, MeshBuffers& bufs, QRhiResourceUpdateBatch& res) const noexcept
{
  SCORE_ASSERT(bufs.buffers.size() == 1);
  auto buf = bufs.buffers[0].handle;
  SCORE_ASSERT(buf);
  res.uploadStaticBuffer(buf, 0, buf->size(), this->vertexArray.data());
}

void BasicMesh::preparePipeline(QRhiGraphicsPipeline& pip) const noexcept
{
  if(cullMode == QRhiGraphicsPipeline::None)
  {
    pip.setDepthTest(false);
    pip.setDepthWrite(false);
  }
  else
  {
    pip.setDepthTest(true);
    pip.setDepthWrite(true);
    // Reverse-Z project rule.
    pip.setDepthOp(QRhiGraphicsPipeline::Greater);
  }

  pip.setTopology(this->topology);
  pip.setCullMode(this->cullMode);
  pip.setFrontFace(this->frontFace);

  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings(this->vertexBindings.begin(), this->vertexBindings.end());
  inputLayout.setAttributes(
      this->vertexAttributes.begin(), this->vertexAttributes.end());
  pip.setVertexInputLayout(inputLayout);
}

void BasicMesh::draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept
{
  SCORE_ASSERT(bufs.buffers.size() == 1);
  auto buf = bufs.buffers[0].handle;
  SCORE_ASSERT(buf);
  SCORE_ASSERT(buf->usage().testFlag(QRhiBuffer::VertexBuffer));
  setupBindings(bufs, cb);

  if(bufs.useIndirectDraw && bufs.indirectDrawBuffer)
  {
    // Fallback ladder — every rung must paint the same pixels; see the
    // RenderState::Caps declaration for the ladder and the producer
    // contract (dead command slots stay zeroed) that makes the
    // full-capacity rungs equivalent to the count rung.
    if(bufs.gpuIndirectCountSupported && bufs.indirectCountBuffer
       && bufs.gpuIndirectSupported)
    {
      if(score::gfx::drawIndirectCountCompat(
             cb, bufs.indirectDrawIndexed, bufs.indirectDrawBuffer,
             bufs.indirectDrawOffset, bufs.indirectCountBuffer,
             bufs.indirectCountOffset, bufs.indirectDrawCount,
             bufs.indirectDrawStride))
        return;
      // API missing in this Qt: fall through to the plain indirect rungs.
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    if(bufs.gpuIndirectSupported)
    {
      if(!bufs.gpuIndirectMultiSupported && bufs.indirectDrawCount > 1)
      {
        // Single-indirect rung: one drawCount=1 call per command. Qt would
        // emulate a multi-draw itself where DrawIndirectMulti is absent, but
        // doing the loop here keeps the rung score-controlled and therefore
        // forceable (SCORE_GFX_NO_GPU_INDIRECT_MULTI) on any hardware.
        for(quint32 i = 0; i < bufs.indirectDrawCount; i++)
        {
          const quint32 off
              = bufs.indirectDrawOffset + i * bufs.indirectDrawStride;
          if(bufs.indirectDrawIndexed)
            cb.drawIndexedIndirect(
                bufs.indirectDrawBuffer, off, 1, bufs.indirectDrawStride);
          else
            cb.drawIndirect(
                bufs.indirectDrawBuffer, off, 1, bufs.indirectDrawStride);
        }
        return;
      }
      if(bufs.indirectDrawIndexed)
        cb.drawIndexedIndirect(
            bufs.indirectDrawBuffer, bufs.indirectDrawOffset,
            bufs.indirectDrawCount, bufs.indirectDrawStride);
      else
        cb.drawIndirect(
            bufs.indirectDrawBuffer, bufs.indirectDrawOffset,
            bufs.indirectDrawCount, bufs.indirectDrawStride);
      return;
    }
#endif
    if(!bufs.cpuDrawCommands.empty())
    {
      int skipped = 0;
      for(const auto& cmd : bufs.cpuDrawCommands)
      {
        if(!drawCommandPaints(cmd))
        {
          ++skipped; // dead slot; see drawCommandPaints
          continue;
        }
        cb.draw(cmd.index_or_vertex_count, cmd.instance_count,
                cmd.first_index_or_vertex, cmd.first_instance);
      }
      noteZeroCountSlotsSkipped(skipped);
      return;
    }
    return; // skip — no commands available yet
  }

  if(vertexCount > 0)
    cb.draw(vertexCount);
}

DummyMesh::DummyMesh(int count)
{
  vertexCount = count;
}

void DummyMesh::setupBindings(
    const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept
{
  cb.setVertexInput(0, 0, nullptr, nullptr);
}

const char* DummyMesh::defaultVertexShader() const noexcept
{
  return R"_(#version 450
out gl_PerVertex { vec4 gl_Position; };

void main()
{
  gl_Position.x = gl_VertexID;
  gl_Position.y = gl_VertexID;
  gl_Position.z = gl_VertexID;
}
)_";
}

void DummyMesh::draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept
{
  cb.setVertexInput(0, 0, nullptr, nullptr);
  cb.draw(vertexCount);
}
PlainMesh::PlainMesh(std::span<const float> vtx, int count)
{
  vertexBindings.push_back({2 * sizeof(float)});
  vertexAttributes.push_back({0, 0, QRhiVertexInputAttribute::Float2, 0});
  vertexArray = vtx;
  vertexCount = count;
}

void PlainMesh::setupBindings(
    const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept
{
  SCORE_ASSERT(bufs.buffers.size() == 1);
  auto buf = bufs.buffers[0].handle;
  SCORE_ASSERT(buf);
  const QRhiCommandBuffer::VertexInput bindings[] = {{buf, 0}};

  cb.setVertexInput(0, 1, bindings);
}

const char* PlainMesh::defaultVertexShader() const noexcept
{
  return R"_(#version 450
layout(location = 0) in vec2 position;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";
}

TexturedMesh::TexturedMesh(std::span<const float> vtx, int count)
{
  vertexBindings.push_back({2 * sizeof(float)});
  vertexBindings.push_back({2 * sizeof(float)});
  vertexAttributes.push_back({0, 0, QRhiVertexInputAttribute::Float2, 0});
  vertexAttributes.push_back({1, 1, QRhiVertexInputAttribute::Float2, 0});
  vertexArray = vtx;
  vertexCount = count;
}

const char* TexturedMesh::defaultVertexShader() const noexcept
{
  return R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";
}

PlainTriangle::PlainTriangle()
    : PlainMesh{data, 3}
{
}

const PlainTriangle& PlainTriangle::instance() noexcept
{
  static const PlainTriangle t;
  return t;
}

TexturedTriangle::TexturedTriangle(bool flipped)
    : TexturedMesh{flipped ? flipped_y_data : data, 3}
{
}

void TexturedTriangle::setupBindings(
    const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept
{
  SCORE_ASSERT(bufs.buffers.size() == 1);
  auto buf = bufs.buffers[0].handle;
  SCORE_ASSERT(buf);

  const QRhiCommandBuffer::VertexInput bindings[]
      = {{buf, 0}, {buf, 3 * 2 * sizeof(float)}};

  cb.setVertexInput(0, 2, bindings);
}

TexturedQuad::TexturedQuad(bool flipped)
    : TexturedMesh{flipped ? flipped_y_data : data, 4}
{
}

void TexturedQuad::setupBindings(
    const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept
{
  SCORE_ASSERT(bufs.buffers.size() == 1);
  auto buf = bufs.buffers[0].handle;
  SCORE_ASSERT(buf);

  const QRhiCommandBuffer::VertexInput bindings[]
      = {{buf, 0}, {buf, 4 * 2 * sizeof(float)}};

  cb.setVertexInput(0, 2, bindings);
}

void drawMeshWithOptionalIndirect(
    const Mesh& mesh, const MeshBuffers& bufs, QRhiCommandBuffer& cb) noexcept
{
  // All Mesh subclasses (BasicMesh, CustomMesh) handle useIndirectDraw
  // internally — they check bufs.useIndirectDraw after binding vertex inputs
  // and dispatch to cb.drawIndirect/drawIndexedIndirect when set — so this
  // helper just forwards to mesh.draw(). It exists as an explicit opt-in
  // marker for renderers that intend to support indirect multi-draw.
  mesh.draw(bufs, cb);
}
}
