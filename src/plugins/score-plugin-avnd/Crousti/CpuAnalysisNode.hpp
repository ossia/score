#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/GfxNode.hpp>

namespace oscr
{

template <typename Node_T>
  requires(
      avnd::texture_input_introspection<Node_T>::size > 0
      && avnd::texture_output_introspection<Node_T>::size == 0)
struct GfxRenderer<Node_T> final : score::gfx::OutputNodeRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  Node_T state;
  score::gfx::Message m_last_message{};
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2>
      m_rts;

  std::vector<QRhiReadbackResult> m_readbacks;
  ossia::time_value m_last_time{-1};

  const GfxNode<Node_T>& node() const noexcept
  {
    return static_cast<const GfxNode<Node_T>&>(score::gfx::NodeRenderer::node);
  }
  GfxRenderer(const GfxNode<Node_T>& p)
      : score::gfx::OutputNodeRenderer{p}
      , m_readbacks(texture_inputs::size)
  {
    prepareNewState(state, p);
  }

  score::gfx::TextureRenderTarget
  renderTargetForInput(const score::gfx::Port& p) override
  {
    auto it = m_rts.find(&p);
    SCORE_ASSERT(it != m_rts.end());
    return it->second;
  }

  template <typename Tex>
  void createInput(
      score::gfx::RenderList& renderer, int k, const Tex& texture_spec,
      const score::gfx::RenderTargetSpecs& spec)
  {
    auto port = this->node().input[k];
    static constexpr auto flags
        = QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource;
    auto texture = renderer.state.rhi->newTexture(
        gpp::qrhi::textureFormat<Tex>(), spec.size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port] = score::gfx::createRenderTarget(
        renderer.state, texture, renderer.samples(), renderer.requiresDepth());
  }

  QRhiTexture* texture(int k) const noexcept
  {
    auto port = this->node().input[k];
    auto it = m_rts.find(port);
    SCORE_ASSERT(it != m_rts.end());
    SCORE_ASSERT(it->second.texture);
    return it->second.texture;
  }

  void loadInputTexture(QRhi& rhi, avnd::cpu_texture auto& cpu_tex, int k)
  {
    auto& buf = m_readbacks[k].data;
    if(buf.size() != cpu_tex.bytesize())
    {
      cpu_tex.bytes = nullptr;
    }
    else
    {
      cpu_tex.bytes = reinterpret_cast<unsigned char*>(buf.data());

      if(rhi.isYUpInNDC())
        if(cpu_tex.width * cpu_tex.height > 0)
          inplaceMirror(
              cpu_tex.bytes, cpu_tex.width, cpu_tex.height, cpu_tex.bytes_per_pixel);

      cpu_tex.changed = true;
    }
  }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    if constexpr(requires { state.prepare(); })
    {
      this->node().processControlIn(
          *this, state, m_last_message, this->node().last_message, this->node().m_ctx);
      state.prepare();
    }

    // Init input render targets
    int k = 0;
    avnd::cpu_texture_input_introspection<Node_T>::for_all(
        avnd::get_inputs<Node_T>(state), [&]<typename F>(F& t) {
      // FIXME k isn't the port index, it's the texture port index
      auto spec = this->node().resolveRenderTargetSpecs(k, renderer);
      if constexpr(requires {
                     t.request_width;
                     t.request_height;
                   })
      {
        spec.size.rwidth() = t.request_width;
        spec.size.rheight() = t.request_height;
      }
      createInput(renderer, k, t.texture, spec);
      if constexpr(avnd::cpu_fixed_format_texture<decltype(t.texture)>)
      {
        t.texture.width = spec.size.width();
        t.texture.height = spec.size.height();
      }
      k++;
    });
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* edge) override
  {
    bool updated = false;
    /*
    int k = 0;
    avnd::cpu_texture_input_introspection<Node_T>::for_all(
        avnd::get_inputs<Node_T>(state), [&]<typename F>(F& t) {
      if constexpr(requires {
                     t.request_width;
                     t.request_height;
                   })
      {
        const auto tex = this->texture(k)->pixelSize();
        if(tex.width() != t.request_width || tex.height() != t.request_height)
        {
          QSize sz{t.request_width, t.request_height};

          // Release
          auto port = parent.input[k];

          m_rts[port].release();
          createInput(renderer, k, t.texture, sz);

          t.texture.width = sz.width();
          t.texture.height = sz.height();

          updated = true;
        }
      }
      k++;
    });
*/
    if(updated)
    {
      // We must notify the graph that the previous nodes have to be recomputed
    }
  }

  void release(score::gfx::RenderList& r) override
  {
    // Free inputs
    // TODO investigate why reference does not work here:
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer, const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    auto& parent = this->node();
    res = renderer.state.rhi->nextResourceUpdateBatch();
    const auto& inputs = parent.input;
    auto index_of_port = ossia::find(inputs, &p) - inputs.begin();
    SCORE_ASSERT(index_of_port == 0);
    {
      auto tex = m_rts[&p].texture;
      auto& readback = m_readbacks[index_of_port];
      readback = {};
      res->readBackTexture(QRhiReadbackDescription{tex}, &readback);
    }
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

    // Fetch input textures (if any)
    {
      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      int k = 0;
      avnd::cpu_texture_input_introspection<Node_T>::for_all(
          avnd::get_inputs<Node_T>(state), [&](auto& t) {
        loadInputTexture(rhi, t.texture, k);
        k++;
      });
    }

    parent.processControlIn(
        *this, state, m_last_message, parent.last_message, parent.m_ctx);

    // Run the processor
    state();

    // Copy the data to the model node
    parent.processControlOut(this->state);

    // Copy the geometry
    // FIXME we need something such as port_run_{pre,post}process for GPU nodes
    avnd::geometry_output_introspection<Node_T>::for_all_n2(
        state.outputs, [&]<std::size_t F, std::size_t P>(
                           auto& t, avnd::predicate_index<P>, avnd::field_index<F>) {
      postprocess_geometry(t);
    });
  }
  template <avnd::geometry_port Field>
  void postprocess_geometry(Field& ctrl)
  {
    using namespace avnd;
    // bool mesh_dirty{};
    // bool tform_dirty{};
    // mesh_dirty = ctrl.dirty_mesh;

    if(ctrl.dirty_mesh)
    {
      auto meshes = std::make_shared<ossia::mesh_list>();
      auto& ossia_meshes = *meshes;
      if constexpr(static_geometry_type<Field> || dynamic_geometry_type<Field>)
      {
        ossia_meshes.meshes.resize(1);
        oscr::load_geometry(ctrl, ossia_meshes.meshes[0]);
      }
      else if constexpr(
          static_geometry_type<decltype(Field::mesh)>
          || dynamic_geometry_type<decltype(Field::mesh)>)
      {
        ossia_meshes.meshes.resize(1);
        oscr::load_geometry(ctrl.mesh, ossia_meshes.meshes[0]);
      }
      else
      {
        oscr::load_geometry(ctrl, ossia_meshes);
      }
    }
    ctrl.dirty_mesh = false;

    // if constexpr(requires { ctrl.transform; })
    // {
    //   if(ctrl.dirty_transform)
    //   {
    //     std::copy_n(
    //         ctrl.transform, std::ssize(ctrl.transform), port.data.transform.matrix);
    //     tform_dirty = true;
    //     ctrl.dirty_transform = false;
    //   }
    // }
    //
    // port.data.flags = {};
    // if(mesh_dirty)
    //   port.data.flags = port.data.flags | ossia::geometry_port::dirty_meshes;
    // if(tform_dirty)
    //   port.data.flags = port.data.flags | ossia::geometry_port::dirty_transform;
  }
};

template <typename Node_T>
  requires(avnd::texture_input_introspection<Node_T>::size > 0
           && avnd::texture_output_introspection<Node_T>::size == 0)
struct GfxNode<Node_T> final
    : CustomGpuOutputNodeBase
    , GpuNodeElements<Node_T>
{
  oscr::ProcessModel<Node_T>& processModel;
  GfxNode(
      oscr::ProcessModel<Node_T>& element,
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls ctls, int id,
      const score::DocumentContext& ctx)
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
