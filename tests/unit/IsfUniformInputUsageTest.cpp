// UNIT — an ISF `uniform_input` must never adopt a buffer that lacks
// QRhiBuffer::UniformBuffer usage.
//
// A `uniform_input` descriptor is VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER. Any
// `storage` RESOURCE exposes a Types::Buffer output, so cabling a storage
// producer into a uniform_input is a graph a user can build in the editor.
// bindUpstreamBuffers() used to adopt whatever buffer the upstream published,
// which makes vkUpdateDescriptorSets reject the write with
// VUID-VkWriteDescriptorSet-descriptorType-00330 and the next
// setShaderResources() segfault; on OpenGL there are no descriptor sets, so
// nothing caught the mismatch and the shader read whatever the binding held.
//
// The decision is pure CPU: bindUpstreamBuffers() either retargets the UBO
// entry at the upstream handle or keeps the zero-filled placeholder. That is
// what is asserted here, on the Null QRhi backend — no GPU, no display. The
// Null backend is legitimate *for this test* precisely because nothing is
// rendered: QRhi::newBuffer() records the usage flags on every backend, and
// the guard reads nothing else.

#include <Gfx/Graph/IsfBindingsBuilder.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <Process/Dataflow/CableData.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QtGlobal>

#include <QtGui/private/qrhi_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
#include <rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhinull_p.h>
#endif

#include <memory>

using namespace score::gfx;

namespace
{
// Publishes a single buffer handle on its (only) output port, the way
// RenderedCSFNode / ExtractBuffer2 / the scene aux extractors do: through
// bufferForOutput(), never through Port::value.
struct UpstreamRenderer final : NodeRenderer
{
  using NodeRenderer::NodeRenderer;

  QRhiBuffer* published{};

  BufferView bufferForOutput(const Port&) override
  {
    return BufferView{.handle = published, .owned = false};
  }

  void init(RenderList&, QRhiResourceUpdateBatch&) override { }
  void update(RenderList&, QRhiResourceUpdateBatch&, Edge*) override { }
  void release(RenderList&) override { }
  void removeOutputPass(RenderList&, Edge&) override { }
};

struct PlainNode final : Node
{
  NodeRenderer* createRenderer(RenderList&) const noexcept override { return nullptr; }
};

// RenderList only serves as the identity that resolves an edge to the upstream
// node's renderer (RenderList::bufferForInput touches no GPU state), and its
// constructor only stores the two references. Nothing here drives the sink.
struct StubOutput final : OutputNode
{
  OutputNodeRenderer* createRenderer(RenderList&) const noexcept override
  {
    return nullptr;
  }
  void setRenderer(std::shared_ptr<RenderList>) override { }
  RenderList* renderer() const override { return nullptr; }
  void startRendering() override { }
  void render() override { }
  void stopRendering() override { }
  bool canRender() const override { return false; }
  void onRendererChange() override { }
  void createOutput(OutputConfiguration) override { }
  void destroyOutput() override { }
  std::shared_ptr<RenderState> renderState() const override { return {}; }
  Configuration configuration() const noexcept override { return {}; }
};

struct Outcome
{
  QRhiBuffer* bound{};       //!< what the UBO entry ended up pointing at
  QRhiBuffer* placeholder{}; //!< the zero-filled UBO the entry started with
};

// One consumer node with a single Buffer input standing for a declared
// `uniform_input`, one producer node publishing `upstream` on its Buffer
// output, cabled together, then bindUpstreamBuffers() run over it.
//
// The placeholder is allocated here rather than by the caller because the
// adopt path takes ownership of it (`if(e.owned && e.buffer)
// e.buffer->deleteLater()`), so only bindUpstreamBuffers' own decision can say
// who frees it.
Outcome bindOutcome(QRhi& rhi, QRhiBuffer* upstream)
{
  StubOutput sink;
  auto st = std::make_shared<RenderState>();
  st->rhi = nullptr; // never dereferenced on this path
  RenderList rl{sink, st};

  // The zero-filled UBO ensureStorageResources() allocates so the binding slot
  // exists even with nothing cabled in.
  QRhiBuffer* placeholder
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64);

  PlainNode producer, consumer;

  // ~Node deletes the ports it holds, so they have to be heap-allocated.
  auto* out = new Port{.node = &producer, .type = Types::Buffer};
  auto* in = new Port{.node = &consumer, .type = Types::Buffer};
  producer.output.push_back(out);
  consumer.input.push_back(in);

  Edge edge{out, in, Process::CableType::ImmediateGlutton};

  UpstreamRenderer up{producer};
  up.published = upstream;
  producer.renderedNodes.insert({&rl, &up});

  GraphicsStorageResources store;
  auto& e = store.ubos.emplace_back();
  e.name = "params";
  e.buffer = placeholder;
  e.owned = true;
  e.binding = 2;
  e.stages = QRhiShaderResourceBinding::FragmentStage;
  e.input_port_index = 0;
  e.declared_size = 64;

  bindUpstreamBuffers(rl, consumer.input, store, nullptr);

  QRhiBuffer* bound = store.ubos.front().buffer;

  // Drop the entry rather than store.release()-ing it: release() would
  // deleteLater() whatever is bound, including an upstream buffer the caller
  // owns. The placeholder is freed here only when it was kept -- on the adopt
  // path bindUpstreamBuffers already handed it to deleteLater().
  store.ubos.front().buffer = nullptr;
  store.ubos.clear();
  producer.renderedNodes.clear();
  if(bound == placeholder)
    placeholder->deleteLater();
  return {bound, placeholder};
}
}

TEST_CASE(
    "a storage buffer is refused at a uniform_input binding",
    "[gfx][isf][binding][regression]")
{
  QRhiNullInitParams params;
  std::unique_ptr<QRhi> rhi{QRhi::create(QRhi::Null, &params)};
  REQUIRE(rhi);

  SECTION("a storage-only upstream keeps the placeholder")
  {
    // What any `storage` RESOURCE publishes on its Types::Buffer output.
    std::unique_ptr<QRhiBuffer> ssbo{
        rhi->newBuffer(QRhiBuffer::Static, QRhiBuffer::StorageBuffer, 64)};
    REQUIRE(ssbo);
    REQUIRE_FALSE(ssbo->usage().testFlag(QRhiBuffer::UniformBuffer));

    // Pre-fix `bound` was ssbo.get(): the storage buffer was written into a
    // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER descriptor.
    const auto o = bindOutcome(*rhi, ssbo.get());
    CHECK(o.bound == o.placeholder);
  }

  SECTION("a vertex/index upstream keeps the placeholder too")
  {
    // Not just the storage case: nothing without UniformBuffer usage may land
    // in a uniform descriptor.
    std::unique_ptr<QRhiBuffer> vbo{
        rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, 64)};
    REQUIRE(vbo);
    const auto o = bindOutcome(*rhi, vbo.get());
    CHECK(o.bound == o.placeholder);
  }

  // Positive control. Without this the two checks above would also pass on a
  // harness that never resolved the upstream at all.
  SECTION("a uniform upstream IS adopted")
  {
    std::unique_ptr<QRhiBuffer> ubo{
        rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64)};
    REQUIRE(ubo);
    const auto o = bindOutcome(*rhi, ubo.get());
    CHECK(o.bound == ubo.get());
  }

  // A buffer carrying both usages is what GraphicsStorageResources' sentinel
  // relies on, and it is legal at a uniform binding.
  SECTION("a combined storage+uniform upstream IS adopted")
  {
    std::unique_ptr<QRhiBuffer> both{rhi->newBuffer(
        QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::UniformBuffer,
        64)};
    REQUIRE(both);
    const auto o = bindOutcome(*rhi, both.get());
    CHECK(o.bound == both.get());
  }
}
