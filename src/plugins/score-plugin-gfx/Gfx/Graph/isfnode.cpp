#include "isfnode.hpp"

#include <score/tools/Debug.hpp>

#include <boost/algorithm/string.hpp>
#include <exprtk.hpp>
namespace
{

struct input_size_vis
{
  int sz{};
  void operator()(const isf::float_input&) noexcept { sz += 4; }

  void operator()(const isf::long_input&) noexcept { sz += 4; }

  void operator()(const isf::event_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::bool_input&) noexcept
  {
    sz += 4; // bool
  }

  void operator()(const isf::point2d_input&) noexcept
  {
    if (sz % 8 != 0)
      sz += 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input&) noexcept
  {
    while (sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 3 * 4;
  }

  void operator()(const isf::color_input&) noexcept
  {
    while (sz % 16 != 0)
    {
      sz += 4;
    }
    sz += 4 * 4;
  }

  void operator()(const isf::image_input&) noexcept
  {
    (*this)(isf::point2d_input{});
  }

  void operator()(const isf::audio_input&) noexcept { }

  void operator()(const isf::audioFFT_input&) noexcept { }
};

struct input_port_vis
{
  ISFNode& self;
  char* data{};
  int sz{};

  void operator()(const isf::float_input& in) noexcept
  {
    if(in.def != 0.)
      *reinterpret_cast<float*>(data) = in.def;
    else
      *reinterpret_cast<float*>(data) = (in.max - in.min) / 2.;
    self.input.push_back(new Port{&self, data, Types::Float, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::long_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = in.def;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::event_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = 0;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::bool_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = in.def;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::point2d_input& in) noexcept
  {
    if (sz % 8 != 0)
    {
      sz += 4;
      data += 4;
    }

    const auto& arr = in.def.value_or(std::array<double, 2>{0.5, 0.5});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    self.input.push_back(new Port{&self, data, Types::Vec2, {}});
    data += 2 * 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input& in) noexcept
  {
    while (sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }
    const auto& arr = in.def.value_or(std::array<double, 3>{0.5, 0.5, 0.5});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    *reinterpret_cast<float*>(data + 8) = arr[2];
    self.input.push_back(new Port{&self, data, Types::Vec3, {}});
    data += 3 * 4;
    sz += 3 * 4;
  }

  void operator()(const isf::color_input& in) noexcept
  {
    while (sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }
    const auto& arr = in.def.value_or(std::array<double, 4>{0.5, 0.5, 0.5, 0.5});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    *reinterpret_cast<float*>(data + 8) = arr[2];
    *reinterpret_cast<float*>(data + 12) = arr[3];
    self.input.push_back(new Port{&self, data, Types::Vec4, {}});
    data += 4 * 4;
    sz += 4 * 4;
  }

  void operator()(const isf::image_input& in) noexcept
  {
    self.input.push_back(new Port{&self, {}, Types::Image, {}});

    // Also add the vec2 imgRect uniform:
    if (sz % 8 != 0)
    {
      sz += 4;
      data += 4;
    }

    *reinterpret_cast<float*>(data) = 640;
    *reinterpret_cast<float*>(data + 4) = 480;
    data += 2 * 4;
    sz += 2 * 4;
  }

  void operator()(const isf::audio_input& audio) noexcept
  {
    self.audio_textures.push_back({});
    auto& data = self.audio_textures.back();
    data.fixedSize = audio.max;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  void operator()(const isf::audioFFT_input&) noexcept { }
};

}

ISFNode::ISFNode(const isf::descriptor& desc, const QShader& vert, const QShader& frag)
    : ISFNode{desc, vert, frag, &TexturedTriangle::instance()}
{
}

ISFNode::ISFNode(const isf::descriptor& desc, const QShader& vert, const QShader& frag, const Mesh* mesh)
    : m_mesh{mesh}
    , m_descriptor{desc}
{
  setShaders(vert, frag);

  // Compoute the size required for the materials
  input_size_vis sz_vis{};

  // Size of the inputs
  for (const isf::input& input : desc.inputs)
  {
    std::visit(sz_vis, input.data);
  }

  // Size of the pass textures
  if(int N = desc.passes.size(); N > 0)
  {
    for(int i = 0; i < N - 1; i++)
      sz_vis(isf::point2d_input{});
  }

  m_materialSize = sz_vis.sz;

  // Allocate the required memory
  // TODO : this must be per-renderer, as the texture sizes may depend on the renderer....
  m_materialData.reset(new char[m_materialSize]);
  std::fill_n(m_materialData.get(), m_materialSize, 0);
  char* cur = m_materialData.get();

  // Create ports pointing to the data used for the UBO
  input_port_vis visitor{*this, cur};
  for (const isf::input& input : desc.inputs)
    std::visit(visitor, input.data);

  // Handle the pass textures size uniforms
  if(int N = desc.passes.size(); N > 0)
  {
    char* data = visitor.data;
    int sz = visitor.sz;
    for(int i = 0; i < N - 1; i++)
    {
      if (sz % 8 != 0)
      {
        sz += 4;
        data += 4;
      }

      *reinterpret_cast<float*>(data) = 640;
      *reinterpret_cast<float*>(data + 4) = 480;
      data += 2 * 4;
      sz += 2 * 4;
    }
  }

  output.push_back(new Port{this, {}, Types::Image, {}});
}

const Mesh& ISFNode::mesh() const noexcept
{
  return *this->m_mesh;
}

struct RenderedISFNode : RenderedNode
{
  struct Pass {
    QRhiSampler* sampler{};
    QRhiTexture* texture{};
    QRhiRenderTarget* rt{};
    QRhiRenderPassDescriptor* rp{};
    QRhiGraphicsPipeline* pipeline{};
    QRhiBuffer* processUBO{};
    QRhiShaderResourceBindings* srb{};
  };
  std::vector<Pass> m_passes;

  using RenderedNode::RenderedNode;

  QSize computeTextureSize(const isf::pass& pass)
  {
    auto& n = (ISFNode&)(node);

    QSize res = m_renderTarget->pixelSize();

    exprtk::symbol_table<float> syms;

    syms.add_constant("var_WIDTH", res.width());
    syms.add_constant("var_HEIGHT", res.height());
    int port_k = 0;
    for(isf::input& input : n.m_descriptor.inputs)
    {
      auto port = node.input[port_k];
      if(std::get_if<isf::float_input>(&input.data))
      {
        syms.add_constant("var_" + input.name, *(float*)port->value);
      }
      else
      {
        // TODO exprtk only handles the expression type...
      }

      port_k++;
    }

    if(auto expr = pass.width_expression; !expr.empty())
    {
      boost::algorithm::replace_all(expr, "$", "var_");
      exprtk::expression<float> e;
      e.register_symbol_table(syms);
      exprtk::parser<float> parser;
      bool ok = parser.compile(expr, e);
      if(ok)
        res.setWidth(e());
      else
        qDebug() << parser.error().c_str() << expr.c_str();
    }
    if(auto expr = pass.height_expression; !expr.empty())
    {
      boost::algorithm::replace_all(expr, "$", "var_");
      exprtk::expression<float> e;
      e.register_symbol_table(syms);
      exprtk::parser<float> parser;
      bool ok = parser.compile(expr, e);
      if(ok)
        res.setHeight(e());
      else
        qDebug() << parser.error().c_str() << expr.c_str();
    }

    return res;
  }


  void customInit(Renderer& renderer) override
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& n = (ISFNode&)(node);
    auto& input = node.input;

    m_materialSize = n.m_materialSize;
    if (m_materialSize > 0)
    {
      m_materialUBO
          = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
      ensure(m_materialUBO->build());
    }

    int cur_pos = 0;
    for (auto in : input)
    {
      switch (in->type)
      {
        case Types::Empty:
          break;
        case Types::Int:
        case Types::Float:
          cur_pos += 4;
          break;
        case Types::Vec2:
          cur_pos += 8;
          if (cur_pos % 8 != 0)
            cur_pos += 4;
          break;
        case Types::Vec3:
          while (cur_pos % 16 != 0)
          {
            cur_pos += 4;
          }
          cur_pos += 12;
          break;
        case Types::Vec4:
          while (cur_pos % 16 != 0)
          {
            cur_pos += 4;
          }
          cur_pos += 16;
          break;
        case Types::Image:
        {
          auto sampler = rhi.newSampler(
              QRhiSampler::Linear,
              QRhiSampler::Linear,
              QRhiSampler::None,
              QRhiSampler::ClampToEdge,
              QRhiSampler::ClampToEdge);
          ensure(sampler->build());

          QRhiTexture* texture = renderer.m_emptyTexture;
          if (!in->edges.empty())
            if (auto source_node = in->edges[0]->source->node)
              if (auto source_rd = source_node->renderedNodes[&renderer])
                if (auto tex = source_rd->m_texture)
                  texture = tex;

          m_samplers.push_back({sampler, texture});

          if (cur_pos % 8 != 0)
            cur_pos += 4;

          *(float*)(node.m_materialData.get() + cur_pos)     = texture->pixelSize().width();
          *(float*)(node.m_materialData.get() + cur_pos + 4) = texture->pixelSize().height();

          cur_pos += 8;
          break;
        }
        default:
          break;
      }
    }

    for (auto& texture : n.audio_textures)
    {
      auto sampler = rhi.newSampler(
          QRhiSampler::Linear,
          QRhiSampler::Linear,
          QRhiSampler::None,
          QRhiSampler::ClampToEdge,
          QRhiSampler::ClampToEdge);
      sampler->build();

      m_samplers.push_back({sampler, renderer.m_emptyTexture});
      texture.samplers[&renderer] = {sampler, nullptr};
    }

    auto& model_passes = n.m_descriptor.passes;
    if(!model_passes.empty())
    {
      int first_pass_sampler_idx = m_samplers.size();
      // First create all the samplers / textures
      for(int i = 0, N = model_passes.size(); i < N - 1; i++)
      {
        auto sampler = rhi.newSampler(
              QRhiSampler::Linear,
              QRhiSampler::Linear,
              QRhiSampler::None,
              QRhiSampler::ClampToEdge,
              QRhiSampler::ClampToEdge);
        sampler->build();

        const QSize texSize = computeTextureSize(model_passes[i]);

        const auto fmt = (model_passes[i].float_storage) ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8;

        auto tex = rhi.newTexture(fmt, texSize, 1, QRhiTexture::Flag{QRhiTexture::RenderTarget});
        tex->build();

        m_samplers.push_back({sampler, tex});

        if (cur_pos % 8 != 0)
          cur_pos += 4;

        *(float*)(node.m_materialData.get() + cur_pos)     = texSize.width();
        *(float*)(node.m_materialData.get() + cur_pos + 4) = texSize.height();

        cur_pos += 8;
      }

      for(int i = 0, N = model_passes.size(); i < N - 1; i++)
      {
        auto [sampler, tex] = m_samplers[first_pass_sampler_idx + i];

        auto rt = rhi.newTextureRenderTarget({tex});
        auto rp = rt->newCompatibleRenderPassDescriptor();
        ensure(rp);
        rt->setRenderPassDescriptor(rp);
        ensure(rt->build());

        auto [ps, srb] = buildPipeline(rhi, node.mesh(), renderer, *rp);

        QRhiBuffer* pubo{};
        // We have to replace the rendered-to texture by an empty one in each pass,
        // as RHI does not support both reading and writing to a texture in the same pass.
        {
          QVarLengthArray<QRhiShaderResourceBinding> bindings;
          for(auto it = srb->cbeginBindings(); it != srb->cendBindings(); ++it)
          {
            bindings.push_back(*it);
            if(it->data()->type == QRhiShaderResourceBinding::SampledTexture)
            {
              if(it->data()->u.stex.texSamplers->tex == tex)
              {
                bindings.back().data()->u.stex.texSamplers->tex = renderer.m_emptyTexture;
              }
            }
            else if(it->data()->type == QRhiShaderResourceBinding::UniformBuffer)
            {
              if(it->data()->u.ubuf.buf == m_processUBO)
              {
                // Allocate a new one instead. Writes to the same buffer get coalesced so
                // we can't have PASSINDEX==0, 1..
                pubo = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
                pubo->build();
                bindings.back().data()->u.ubuf.buf = pubo;
              }
            }
          }
          srb->setBindings(bindings.begin(), bindings.end());
          srb->build();
        }
        m_passes.push_back({sampler, tex, rt, rp, ps, pubo, srb});
      }

      // Last pass is the main write
      auto [ps, srb] = buildPipeline(rhi, node.mesh(), renderer, *m_renderPass);
      m_ps = ps;
      m_srb = srb;
      m_passes.push_back({nullptr, nullptr, m_renderTarget, m_renderPass, m_ps, m_processUBO, m_srb});
    }
  }

  void customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) override
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& n = (ISFNode&)node;
    for (auto& audio : n.audio_textures)
    {
      bool textureChanged = false;
      auto& [rhiSampler, rhiTexture] = audio.samplers[&renderer];
      const auto curSz = (rhiTexture) ? rhiTexture->pixelSize() : QSize{};
      int numSamples = curSz.width() * curSz.height();
      if (numSamples != audio.data.size())
      {
        delete rhiTexture;
        rhiTexture = nullptr;
        textureChanged = true;
      }

      if (!rhiTexture)
      {
        if (audio.channels > 0)
        {
          int samples = audio.data.size() / audio.channels;
          rhiTexture = rhi.newTexture(
              QRhiTexture::D32F, {samples, audio.channels}, 1, QRhiTexture::Flag{});
          rhiTexture->build();
          textureChanged = true;
        }
        else
        {
          rhiTexture = nullptr;
          textureChanged = true;
        }
      }

      if (textureChanged)
      {
        replaceTexture(rhiSampler, rhiTexture ? rhiTexture : renderer.m_emptyTexture);
      }

      if (rhiTexture)
      {
        QRhiTextureSubresourceUploadDescription subdesc(audio.data.data(), audio.data.size() * 4);
        QRhiTextureUploadEntry entry{0, 0, subdesc};
        QRhiTextureUploadDescription desc{entry};
        res.uploadTexture(rhiTexture, desc);
      }
    }

    if(!m_passes.empty())
    {
      // Update all the process UBOs
      for(int i = 0, N = m_passes.size(); i < N - 1; i++)
      {
        ((NodeModel&)node).standardUBO.passIndex = i;
        res.updateDynamicBuffer(m_passes[i].processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);
      }
    }
  }

  void customRelease(Renderer& renderer) override
  {
    auto& n = (ISFNode&)(node);
    for (auto& texture : n.audio_textures)
      if (auto tex = texture.samplers[&renderer].second)
      {
        if (tex != renderer.m_emptyTexture)
          tex->releaseAndDestroyLater();
      }

    if(!m_passes.empty())
    {
      // Last pass is the "main pass" already being released.
      m_passes.pop_back();
    }

    for (auto& pass : m_passes)
    {
      pass.rt->releaseAndDestroyLater();
      pass.rp->releaseAndDestroyLater();
      pass.texture->releaseAndDestroyLater();
      pass.processUBO->releaseAndDestroyLater();
      pass.srb->releaseAndDestroyLater();
      pass.pipeline->releaseAndDestroyLater();
    }

    m_passes.clear();
  }

  void runPass(Renderer& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch& res) override
  {
    if(m_passes.empty())
      return RenderedNode::runPass(renderer, cb, res);

    // Update a first time everything

    // PASSINDEX must be set to the last index
    // FIXME
    ((NodeModel&)node).standardUBO.passIndex = m_passes.size() - 1;

    update(renderer, res);

    auto updateBatch = &res;

    // Draw the passes
    for(const auto& pass : m_passes)
    {
      SCORE_ASSERT(pass.rt);
      SCORE_ASSERT(pass.pipeline);
      SCORE_ASSERT(pass.srb);
      // TODO : combine all the uniforms..

      // TODO need to free stuff
      cb.beginPass(pass.rt, Qt::black, {1.0f, 0}, updateBatch);
      {
        cb.setGraphicsPipeline(pass.pipeline);
        cb.setShaderResources(pass.srb);

        if(pass.texture)
        {
          cb.setViewport(QRhiViewport(0, 0, pass.texture->pixelSize().width(), pass.texture->pixelSize().height()));
        }
        else
        {
          const auto sz = renderer.state.size;
          cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
        }

        assert(this->m_meshBuffer);
        assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
        node.mesh().setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

        cb.draw(node.mesh().vertexCount);
      }

      cb.endPass();

       if(pass.pipeline != m_ps)
       {
         // Not the last pass: we have to use another resource batch
         updateBatch = renderer.state.rhi->nextResourceUpdateBatch();
       }
    }
  }
};

RenderedNode* ISFNode::createRenderer() const noexcept
{
  return new RenderedISFNode{*this};
}
