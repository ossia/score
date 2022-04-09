#pragma once

#if SCORE_PLUGIN_GFX
#include <Crousti/Concepts.hpp>
#include <Crousti/Metadatas.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderState.hpp>
#include <Gfx/Graph/Uniforms.hpp>

#include <avnd/common/for_nth.hpp>

namespace oscr
{

struct GenericTexgenNode : score::gfx::NodeModel
{
  static const constexpr auto vertex = R"_(#version 450
  layout(location = 0) in vec2 position;
  layout(location = 1) in vec2 texcoord;

  layout(binding=3) uniform sampler2D y_tex;
  layout(location = 0) out vec2 v_texcoord;

  layout(std140, binding = 0) uniform renderer_t {
    mat4 clipSpaceCorrMatrix;
    vec2 texcoordAdjust;
    vec2 renderSize;
  };

  out gl_PerVertex { vec4 gl_Position; };

  void main()
  {
    v_texcoord = texcoord;
    gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
  }
  )_";

  static const constexpr auto filter = R"_(#version 450
  layout(location = 0) in vec2 v_texcoord;
  layout(location = 0) out vec4 fragColor;

  layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
  };

  layout(binding=3) uniform sampler2D y_tex;


  void main ()
  {
    vec2 texcoord = vec2(v_texcoord.x, texcoordAdjust.y + texcoordAdjust.x * v_texcoord.y);
    fragColor = texture(y_tex, texcoord);
  }
  )_";

  struct ubo
  {
    int currentImageIndex{};
    float pad;
    float position[2];
  } ubo;

  QImage image;
  GenericTexgenNode()
  {
    std::tie(m_vertexS, m_fragmentS) = score::gfx::makeShaders(vertex, filter);
  }

  score::gfx::Message last_message;
  void process(const score::gfx::Message& msg) override
  {
    ProcessNode::process(msg.token);
    last_message.token = msg.token;
    if(last_message.input.empty())
    {
      last_message = msg;
    }
    else
    {
      for(std::size_t i = 0; i < msg.input.size(); i++)
      {
        // If there's some data, overwrite it
        if(msg.input[i].index() != 0)
          last_message.input[i] = msg.input[i];
      }
    }
  }

  virtual ~GenericTexgenNode() { m_materialData.release(); }
};

#include <Gfx/Qt5CompatPush> // clang-format: keep
struct GenericTexgenRenderer : score::gfx::GenericNodeRenderer
{
  using GenericNodeRenderer::GenericNodeRenderer;

  ~GenericTexgenRenderer() { }


  void update(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res) override
  {
    defaultUBOUpdate(renderer, res);
  }

};

template<typename Node_T>
struct GfxRenderer final : GenericTexgenRenderer
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  const GenericTexgenNode& parent;
  Node_T& state;
  ossia::small_flat_map<const score::gfx::Port*, score::gfx::TextureRenderTarget, 2> m_rts;

  std::vector<QRhiReadbackResult> m_readbacks;
  ossia::time_value m_last_time{-1};

  GfxRenderer(const GenericTexgenNode& p, Node_T& node)
      : GenericTexgenRenderer{p}
      , parent{p}
      , state{node}
      , m_readbacks{texture_inputs::size}
  {
  }

  score::gfx::TextureRenderTarget renderTargetForInput(const score::gfx::Port& p) override
  {
    auto it = m_rts.find(&p);
    SCORE_ASSERT(it != m_rts.end());
    return it->second;
  }

  void createInput(score::gfx::RenderList& renderer, int k, QSize size)
  {
    auto port = parent.input[k];
    constexpr auto flags = QRhiTexture::RenderTarget |  QRhiTexture::UsedAsTransferSource;
    auto texture = renderer.state.rhi->newTexture(QRhiTexture::RGBA8, size, 1, flags);
    SCORE_ASSERT(texture->create());
    m_rts[port] = score::gfx::createRenderTarget(renderer.state, texture);
  }

  void createOutput(score::gfx::RenderList& renderer, QSize size)
  {
    auto& rhi = *renderer.state.rhi;
    QRhiTexture* texture = &renderer.emptyTexture();
    if(size.width() > 0 && size.height() > 0)
    {
      texture = rhi.newTexture(
        QRhiTexture::RGBA8, size, 1, QRhiTexture::Flag{});

      texture->create();
    }

    auto sampler = rhi.newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge);

    sampler->create();
    m_samplers.push_back({sampler, texture});
  }

  QRhiTexture* updateTexture(score::gfx::RenderList& renderer, int k, const avnd::cpu_texture auto& cpu_tex)
  {
    auto& [sampler, texture] = m_samplers[k];
    if(texture)
    {
      auto sz = texture->pixelSize();
      if(cpu_tex.width == sz.width() && cpu_tex.height == sz.height())
        return texture;
    }

    // Check the texture size
    if(cpu_tex.width > 0 && cpu_tex.height > 0)
    {
      QRhiTexture* oldtex = texture;
      QRhiTexture* newtex = renderer.state.rhi->newTexture(
          QRhiTexture::RGBA8, QSize{cpu_tex.width, cpu_tex.height}, 1, QRhiTexture::Flag{});
      newtex->create();
      for(auto& [edge, pass] : this->m_p)
        if(pass.srb)
          score::gfx::replaceTexture(*pass.srb, sampler, newtex);
      texture = newtex;

      if(oldtex && oldtex != &renderer.emptyTexture())
      {
        oldtex->deleteLater();
      }

      return newtex;
    }
    else
    {
      for(auto& [edge, pass] : this->m_p)
        if(pass.srb)
          score::gfx::replaceTexture(*pass.srb, sampler, &renderer.emptyTexture());

      return &renderer.emptyTexture();
    }
  }

  void uploadOutputTexture(
      score::gfx::RenderList& renderer,
      int k,
      avnd::cpu_texture auto& cpu_tex,
      QRhiResourceUpdateBatch* res)
  {
    if(cpu_tex.changed)
    {
      if(auto texture = updateTexture(renderer, k, cpu_tex))
      {
        // Upload it
        {
          QRhiTextureSubresourceUploadDescription sd{cpu_tex.bytes, cpu_tex.width * cpu_tex.height * 4};
          QRhiTextureUploadDescription desc{QRhiTextureUploadEntry{0, 0, sd}};

          res->uploadTexture(texture, desc);
        }

        cpu_tex.changed = false;
        k++;
      }
    }
  }

  void loadInputTexture(avnd::cpu_texture auto& cpu_tex, int k)
  {
    auto& buf = m_readbacks[k].data;
    if(buf.size() != 4 * cpu_tex.width * cpu_tex.height)
    {
      cpu_tex.bytes = nullptr;
    }
    else
    {
      cpu_tex.bytes = reinterpret_cast<unsigned char*>(buf.data());
      cpu_tex.changed = true;
    }
  }

  void init(score::gfx::RenderList& renderer) override
  {
    const auto& mesh = renderer.defaultTriangle();
    defaultMeshInit(renderer, mesh);
    processUBOInit(renderer);
    m_material.init(renderer, node.input, m_samplers);

    if constexpr(texture_inputs::size > 0)
    {
      // Init input render targets
      int k = 0;
      avnd::cpu_texture_input_introspection<Node_T>::for_all(
            avnd::get_inputs<Node_T>(state), [&] (auto& t) {
        auto sz = renderer.state.size;
        createInput(renderer, k, sz);
        t.texture.width = sz.width();
        t.texture.height = sz.height();
        k++;
      });
    }

    if constexpr(texture_outputs::size > 0)
    {
      // Init textures for the outputs
      int k = 0;
      avnd::cpu_texture_output_introspection<Node_T>::for_all(
            avnd::get_outputs<Node_T>(state), [&] (auto& t) {
        createOutput(renderer, QSize{t.texture.width, t.texture.height});
        k++;
      });
    }

    defaultPassesInit(renderer, mesh);
  }

  void release(score::gfx::RenderList& r) override
  {
    // Free outputs
    for(auto& [sampl, texture] : m_samplers)
    {
      if(texture != &r.emptyTexture())
        texture->deleteLater();
      texture = nullptr;
    }

    // Free inputs
    // TODO investigate why reference does not work here:
    for(auto [port, rt] : m_rts)
      rt.release();
    m_rts.clear();

    defaultRelease(r);
  }

  void inputAboutToFinish(
      score::gfx::RenderList& renderer,
      const score::gfx::Port& p,
      QRhiResourceUpdateBatch*& res) override
  {
    res = renderer.state.rhi->nextResourceUpdateBatch();
    const auto& inputs = this->node.input;
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
      score::gfx::RenderList& renderer,
      QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res,
      score::gfx::Edge& edge) override
  {
    auto& rhi = *renderer.state.rhi;

    // If we are paused, we don't run the processor implementation.
    if(parent.last_message.token.date == m_last_time)
    {
      return;
    }
    m_last_time = parent.last_message.token.date;

    // Fetch input textures (if any)
    if constexpr(texture_inputs::size > 0)
    {
      // Insert a synchronisation point to allow readbacks to complete
      rhi.finish();

      // Copy the readback output inside the structure
      // TODO it would be much better to do this inside the readback's
      // "completed" callback.
      int k = 0;
      avnd::cpu_texture_input_introspection<Node_T>::for_all(
            avnd::get_inputs<Node_T>(state), [&] (auto& t) {
        loadInputTexture(t.texture, k);
        k++;
      });
    }

    // Apply the controls
    std::size_t k = 0;
    avnd::parameter_input_introspection<Node_T>::for_all(
          avnd::get_inputs<Node_T>(state),
          [&] (avnd::parameter auto& t) {
            auto& mess = this->parent.last_message;
            if(mess.input.size() > k)
            {
              if(auto val = std::get_if<ossia::value>(&mess.input[k]))
              {
                using type = std::remove_reference_t<decltype(t.value)>;
                t.value = ossia::convert<type>(*val);
              }
            }
            k++;
          }
    );


    // Run the processor
    state();

    // Upload output textures
    if constexpr(texture_outputs::size > 0)
    {
      int k = 0;
      avnd::cpu_texture_output_introspection<Node_T>::for_all(
            avnd::get_outputs<Node_T>(state), [&] (auto& t) {
        uploadOutputTexture(renderer, k, t.texture, res);
        k++;
      });

      commands.resourceUpdate(res);
      res = renderer.state.rhi->nextResourceUpdateBatch();
    }
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep
template<typename Node_T>
struct GfxNode final : GenericTexgenNode
{
  using texture_inputs = avnd::texture_input_introspection<Node_T>;
  using texture_outputs = avnd::texture_output_introspection<Node_T>;
  std::shared_ptr<Node_T> node;

  GfxNode(std::shared_ptr<Node_T> n)
      : node{std::move(n)}
  {
    for(std::size_t i = 0; i < texture_inputs::size; i++)
    {
      input.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }
    for(std::size_t i = 0; i < texture_outputs::size; i++)
    {
      output.push_back(new score::gfx::Port{this, {}, score::gfx::Types::Image, {}});
    }
  }

  score::gfx::NodeRenderer*
  createRenderer(score::gfx::RenderList& r) const noexcept override
  {
    return new GfxRenderer<Node_T>{*this, *node};
  }
};

}
#endif
