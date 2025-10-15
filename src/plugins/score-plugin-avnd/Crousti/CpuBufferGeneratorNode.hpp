#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
      (avnd::texture_input_introspection<Node_T>::size == 0 && avnd::buffer_input_introspection<Node_T>::size == 0)
      && (avnd::texture_output_introspection<Node_T>::size == 0 && avnd::buffer_output_introspection<Node_T>::size > 0))
struct GfxRenderer<Node_T> final : score::gfx::GenericNodeRenderer
{
  using buffer_outputs = avnd::buffer_output_introspection<Node_T>;
  std::shared_ptr<Node_T> state;
  score::gfx::Message m_last_message{};
  std::vector<std::pair<const score::gfx::Port*, QRhiBuffer*>> m_buffers;
  ossia::time_value m_last_time{-1};


  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }

  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::GenericNodeRenderer{p}
      , state{std::make_shared<Node_T>()}
  {
    prepareNewState<Node_T>(state, p);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  QRhiBuffer* bufferForOutput(const score::gfx::Port& output) {
    for(auto& [p, b] : m_buffers)
      if(p == &output)
        return b;
    return nullptr;
  }

  void
  createOutput(score::gfx::RenderList& renderer, score::gfx::Port& port, const avnd::cpu_raw_buffer auto& cpu_buf)
  {
    auto& rhi = *renderer.state.rhi;
    QRhiBuffer* buffer = nullptr;
    if(cpu_buf.bytesize > 0)
    {
      buffer = rhi.newBuffer(
          QRhiBuffer::Dynamic
          , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
          , cpu_buf.bytesize);

      buffer->create();
    }

    this->m_buffers.emplace_back(&port, buffer);
  }

  void uploadOutputBuffer(
      score::gfx::RenderList& renderer, int k, avnd::cpu_raw_buffer auto& cpu_buf,
      QRhiResourceUpdateBatch& res)
  {
    if(cpu_buf.changed)
    {
      assert(m_buffers.size() > k);
      auto& [port, buf] = m_buffers[k];
      if(!buf)
      {
        if(cpu_buf.bytesize > 0)
        {
          buf = renderer.state.rhi->newBuffer(
              QRhiBuffer::Dynamic
              , QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer
              , cpu_buf.bytesize);

          buf->create();
        }
      }
      else if(buf->size() != cpu_buf.bytesize)
      {
        buf->destroy();
        buf->setSize(cpu_buf.bytesize);
        buf->create();
      }
      res.updateDynamicBuffer(buf, 0, cpu_buf.bytesize, cpu_buf.bytes);
      cpu_buf.changed = false;
      k++;
    }
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    auto& parent = node();
    if constexpr(requires { state->prepare(); })
    {
      parent.processControlIn(
          *this, *state, m_last_message, parent.last_message, parent.m_ctx);
      state->prepare();
    }

    // Init buffers for the outputs
    avnd::buffer_output_introspection<Node_T>::for_all_n2(
        avnd::get_outputs<Node_T>(*state), [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
          SCORE_ASSERT(parent.output.size() > nf);
          SCORE_ASSERT(parent.output[nf]->type == score::gfx::Types::Buffer);
          createOutput(renderer, *parent.output[nf], port.buffer);
        });
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    auto& parent = node();
    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
      return;
    m_last_time = parent.last_message.token.date;

    parent.processControlIn(
        *this, *state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the processor
    if_possible((*state)());

    // Upload output buffers
    int k = 0;
    avnd::buffer_output_introspection<Node_T>::for_all(
        avnd::get_outputs<Node_T>(*state), [&](auto& t) {
      uploadOutputBuffer(renderer, k, t.buffer, res);
      k++;
    });

    // Copy the data to the model node
    parent.processControlOut(*this->state);
  }

  void release(score::gfx::RenderList& r) override
  {
    // Free outputs
    for(auto& [p, buf] : this->m_buffers)
    {
      if(buf)
      {
        buf->destroy();
        buf->deleteLater();
      }
    }
    this->m_buffers.clear();
  }
};

template <typename Node_T>
  requires(
              (avnd::texture_input_introspection<Node_T>::size == 0 && avnd::buffer_input_introspection<Node_T>::size == 0)
              && (avnd::texture_output_introspection<Node_T>::size == 0 && avnd::buffer_output_introspection<Node_T>::size > 0))
struct GfxNode<Node_T> final
    : CustomGfxNodeBase
    , GpuWorker
    , GpuControlIns
    , GpuControlOuts
    , GpuNodeElements<Node_T>
{
  oscr::ProcessModel<Node_T>& processModel;
  GfxNode(
      oscr::ProcessModel<Node_T>& element,
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls,
      int64_t id, const score::DocumentContext& ctx)
      : CustomGfxNodeBase{ctx}
      , GpuControlOuts{std::move(q), std::move(ctls)}
      , processModel{element}
  {
    this->instance = id;

    initGfxPorts<Node_T>(this, this->input, this->output);
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new GfxRenderer<Node_T>{*this};
  }
};
}
#endif
