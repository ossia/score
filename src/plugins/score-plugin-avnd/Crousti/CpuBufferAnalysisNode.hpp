#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
      (avnd::texture_input_introspection<Node_T>::size == 0 && avnd::buffer_input_introspection<Node_T>::size > 0)
      && (avnd::texture_output_introspection<Node_T>::size == 0 && avnd::buffer_output_introspection<Node_T>::size == 0))
struct GfxRenderer<Node_T> final : score::gfx::OutputNodeRenderer
{
  using buffer_inputs = avnd::buffer_input_introspection<Node_T>;
  std::shared_ptr<Node_T> state;
  score::gfx::Message m_last_message{};
  std::vector<QRhiReadbackResult> m_readbacks;
  ossia::time_value m_last_time{-1};

  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }
  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::OutputNodeRenderer{p}
      , state{std::make_shared<Node_T>()}
     , m_readbacks(buffer_inputs::size)
  {
    prepareNewState<Node_T>(state, p);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    return {};
  }

  void readbackInputBuffer(score::gfx::RenderList& renderer,  QRhiResourceUpdateBatch& res,  int port_index, int buffer_index)
  {
    // FIXME: instead of doing this we could do the readback in the
    // producer node and just read its bytearray once...
    auto& parent = this->node();
    const auto& inputs = parent.input;
    SCORE_ASSERT(port_index == 0);
    {
      score::gfx::Port* p = inputs[port_index];
      for(auto& edge : p->edges)
      {
        auto src_node = edge->source->node;
        score::gfx::NodeRenderer* src_renderer = src_node->renderedNodes.at(&renderer);
        if(src_renderer)
        {
          auto buf = src_renderer->bufferForOutput(*edge->source);
          if(buf)
          {
            auto& readback = m_readbacks[buffer_index];
            readback = {};
            res.readBackBuffer(buf, 0, buf->size(), &readback);
          }
        }
        break;
      }
    }
  }

  void loadInputBuffer(QRhi& rhi, avnd::cpu_raw_buffer auto& cpu_tex, int k)
  {
    SCORE_ASSERT(k < m_readbacks.size());
    auto& buf = m_readbacks[k].data;
    cpu_tex.bytes = reinterpret_cast<unsigned char*>(buf.data());
    cpu_tex.bytesize = buf.size();
    cpu_tex.changed = true;
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if constexpr(requires { state->prepare(); })
    {
      this->node().processControlIn(
          *this, *state, m_last_message, this->node().last_message, this->node().m_ctx);
      state->prepare();
    }
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
  }

  void release(score::gfx::RenderList& r) override
  {
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    res = renderer.state.rhi->nextResourceUpdateBatch();
    avnd::buffer_input_introspection<Node_T>::for_all_n2(
        avnd::get_inputs<Node_T>(*state),
        [&]<typename Field, std::size_t N, std::size_t NField>
        (Field& port, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
      readbackInputBuffer(renderer, *res, nf, np);
    });
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge) override
  {
    auto& parent = this->node();
    auto& rhi = *renderer.state.rhi;

    // Insert a synchronisation point to allow readbacks to complete
    rhi.finish();

    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
    {
      return;
    }
    m_last_time = parent.last_message.token.date;

    // Fetch input buffers (if any)
    {
      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      avnd::buffer_input_introspection<Node_T>::for_all_n2(
          avnd::get_inputs<Node_T>(*state),
          [&]<typename Field, std::size_t N, std::size_t NField>
          (Field& t, avnd::predicate_index<N> np, avnd::field_index<NField> nf) {
            loadInputBuffer(rhi, t.buffer, np);
          });
    }

    parent.processControlIn(
        *this, *state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the processor
    if_possible((*state)());

    // Copy the data to the model node
    parent.processControlOut(*this->state);

    // No output buffer

    // Copy the geometry
    // FIXME we need something such as port_run_{pre,post}process for GPU nodes
    avnd::geometry_output_introspection<Node_T>::for_all_n2(
        state->outputs, [&]<std::size_t F, std::size_t P>(
                            auto& t, avnd::predicate_index<P>, avnd::field_index<F>) {
          postprocess_geometry(t);
        });
  }

  template <avnd::geometry_port Field>
  static void postprocess_geometry(Field& ctrl)
  {
    // FIXME
  }
};

template <typename Node_T>
  requires(
              (avnd::texture_input_introspection<Node_T>::size == 0 && avnd::buffer_input_introspection<Node_T>::size > 0)
              && (avnd::texture_output_introspection<Node_T>::size == 0 && avnd::buffer_output_introspection<Node_T>::size == 0))
struct GfxNode<Node_T> final
    : CustomGpuOutputNodeBase
    , GpuNodeElements<Node_T>
{
  oscr::ProcessModel<Node_T>& processModel;
  GfxNode(
      oscr::ProcessModel<Node_T>& element,
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls,
      int64_t id, const score::DocumentContext& ctx)
      : CustomGpuOutputNodeBase{std::move(q), std::move(ctls), ctx}
      , processModel{element}
  {
    this->instance = id;

    initGfxPorts<Node_T>(this, this->input, this->output);
  }

  score::gfx::OutputNodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new GfxRenderer<Node_T>{*this};
  }
};
}
#endif
