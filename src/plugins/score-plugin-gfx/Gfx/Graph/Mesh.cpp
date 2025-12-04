#include <Gfx/Graph/Mesh.hpp>

namespace score::gfx
{

Mesh::Mesh() = default;

Mesh::~Mesh() = default;

MeshBuffers BasicMesh::init(QRhi& rhi) const noexcept
{
  auto mesh_buf = rhi.newBuffer(
      QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer,
      vertexArray.size() * sizeof(float));
  mesh_buf->setName("Mesh::mesh_buf");
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
}
