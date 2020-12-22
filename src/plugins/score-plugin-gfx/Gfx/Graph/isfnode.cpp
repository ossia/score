#include "isfnode.hpp"

#include <score/tools/Debug.hpp>

#include <boost/algorithm/string.hpp>
#include <ossia/math/math_expression.hpp>
#include <ossia/audio/fft.hpp>
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

  void operator()(const isf::audioFFT_input& audio) noexcept
  {
    self.audio_textures.push_back({});
    auto& data = self.audio_textures.back();
    data.fixedSize = audio.max;
    data.fft = true;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }
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
  m_vertexS = vert;
  m_fragmentS = frag;

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

struct RenderedISFNode : score::gfx::NodeRenderer
{
  struct Pass {
    QRhiSampler* sampler{};
    TextureRenderTarget renderTarget;
    Pipeline p;
    QRhiBuffer* processUBO{};
  };
  std::vector<Pass> m_passes;

  //std::unique_ptr<char[]> m_materialData;

  ISFNode& n;


  TextureRenderTarget m_lastPassRT;

  std::vector<Sampler> m_samplers;

  QRhiBuffer* m_meshBuffer{};
  QRhiBuffer* m_idxBuffer{};

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};
  int64_t materialChangedIndex{-1};

  std::vector<float> m_audioScratchpad;

  RenderedISFNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{}
    , n{const_cast<ISFNode&>(node)}
    , m_fft{128}
  {

  }

  virtual ~RenderedISFNode();
  std::optional<QSize> renderTargetSize() const noexcept override
  {
    return {};
  }

  TextureRenderTarget createRenderTarget(const RenderState& state) override
  {
    auto sz = state.size;
    if (auto true_sz = renderTargetSize())
    {
      sz = *true_sz;
    }

    m_lastPassRT = score::gfx::createRenderTarget(state, sz);
    return m_lastPassRT;
  }

  QSize computeTextureSize(const isf::pass& pass)
  {
    QSize res = m_lastPassRT.renderTarget->pixelSize();

    ossia::math_expression e;
    ossia::small_pod_vector<double, 16> data;

    // Note : reserve is super important here,
    // as the expression parser takes *references* to the
    // variables.
    data.reserve(2 + n.m_descriptor.inputs.size());

    e.add_constant("var_WIDTH", data.emplace_back(res.width()));
    e.add_constant("var_HEIGHT", data.emplace_back(res.height()));
    int port_k = 0;
    for(const isf::input& input : n.m_descriptor.inputs)
    {
      auto port = n.input[port_k];
      if(std::get_if<isf::float_input>(&input.data))
      {
        e.add_constant("var_" + input.name, data.emplace_back(*(float*)port->value));
      }
      else if(std::get_if<isf::long_input>(&input.data))
      {
        e.add_constant("var_" + input.name, data.emplace_back(*(int*)port->value));
      }

      port_k++;
    }

    if(auto expr = pass.width_expression; !expr.empty())
    {
      boost::algorithm::replace_all(expr, "$", "var_");
      e.register_symbol_table();
      bool ok = e.set_expression(expr);
      if(ok)
        res.setWidth(e.value());
      else
        qDebug() << e.error().c_str() << expr.c_str();
    }
    if(auto expr = pass.height_expression; !expr.empty())
    {
      boost::algorithm::replace_all(expr, "$", "var_");
      e.register_symbol_table();
      bool ok = e.set_expression(expr);
      if(ok)
        res.setHeight(e.value());
      else
        qDebug() << e.error().c_str() << expr.c_str();
    }

    return res;
  }

  int initShaderSamplers(Renderer& renderer)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& input = n.input;
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
          SCORE_ASSERT(sampler->build());

          auto texture = renderer.textureTargetForInputPort(*in);
          m_samplers.push_back({sampler, texture});

          if (cur_pos % 8 != 0)
            cur_pos += 4;

          *(float*)(n.m_materialData.get() + cur_pos)     = texture->pixelSize().width();
          *(float*)(n.m_materialData.get() + cur_pos + 4) = texture->pixelSize().height();

          cur_pos += 8;
          break;
        }
        default:
          break;
      }
    }
    return cur_pos;
  }

  void initAudioTextures(Renderer& renderer)
  {
    QRhi& rhi = *renderer.state.rhi;
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
  }

  void initPassSamplers(Renderer& renderer, int& cur_pos)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto& model_passes = n.m_descriptor.passes;
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

      *(float*)(n.m_materialData.get() + cur_pos)     = texSize.width();
      *(float*)(n.m_materialData.get() + cur_pos + 4) = texSize.height();

      cur_pos += 8;
    }
  }

  Pipeline buildPassPipeline(Renderer& renderer, TextureRenderTarget tgt, QRhiBuffer* processUBO) {
    return score::gfx::buildPipeline(renderer,  n.mesh(), n.m_vertexS, n.m_fragmentS, tgt, processUBO, m_materialUBO, m_samplers);
  };

  Pass createPass(Renderer& renderer, Sampler target)
  {
    QRhi& rhi = *renderer.state.rhi;
    auto [sampler, tex] = target;

    auto rt = rhi.newTextureRenderTarget({tex});
    auto rp = rt->newCompatibleRenderPassDescriptor();
    SCORE_ASSERT(rp);
    rt->setRenderPassDescriptor(rp);
    SCORE_ASSERT(rt->build());

    QRhiBuffer* pubo{};
    pubo = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
    pubo->build();

    auto pip = buildPassPipeline(renderer, TextureRenderTarget{tex, rp, rt}, pubo);
    auto srb = pip.srb;

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
      }
      srb->setBindings(bindings.begin(), bindings.end());
      srb->build();
    }
    return Pass{sampler, {tex, rp, rt}, pip, pubo};
  }

  void init(Renderer& renderer) override
  {
    // init()
    {
      const auto& mesh = n.mesh();
      if (!m_meshBuffer)
      {
        auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
        m_meshBuffer = mbuffer;
        m_idxBuffer = ibuffer;
      }
    }

    QRhi& rhi = *renderer.state.rhi;

    m_materialSize = n.m_materialSize;
    if (m_materialSize > 0)
    {
      m_materialUBO
          = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
      SCORE_ASSERT(m_materialUBO->build());
    }

    int cur_pos = initShaderSamplers(renderer);

    initAudioTextures(renderer);


    auto& model_passes = n.m_descriptor.passes;
    if(!model_passes.empty())
    {
      int first_pass_sampler_idx = m_samplers.size();

      // First create all the samplers / textures
      initPassSamplers(renderer, cur_pos);

      // Then create the passes
      for(int i = 0, N = model_passes.size(); i < N - 1; i++)
      {
        auto target = m_samplers[first_pass_sampler_idx + i];
        auto pass = createPass(renderer, target);
        m_passes.push_back(pass);
      }
    }

    // Last pass is the main write
    {
      QRhiBuffer* pubo{};
      pubo = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
      pubo->build();

      auto p = buildPassPipeline(renderer, m_lastPassRT, pubo);
      m_passes.push_back(Pass{nullptr, m_lastPassRT, p, pubo});
    }
  }

  void updateAudioTexture(AudioTexture& audio, Renderer& renderer, QRhiResourceUpdateBatch& res)
  {
    QRhi& rhi = *renderer.state.rhi;
    bool textureChanged = false;
    auto it = audio.samplers.find(&renderer);
    if(it == audio.samplers.end())
      return;

    auto& [rhiSampler, rhiTexture] = it->second;
    const auto curSz = (rhiTexture) ? rhiTexture->pixelSize() : QSize{};
    int numSamples = curSz.width() * curSz.height();
    if (numSamples != int(audio.data.size()))
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
        int pixelWidth = samples / (audio.fft ? 2 : 1);
        m_fft.reset(samples);
        rhiTexture = rhi.newTexture(
              QRhiTexture::R32F, {pixelWidth, audio.channels}, 1, QRhiTexture::Flag{});
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
      for(auto& m_p : m_passes)
      score::gfx::replaceTexture(*m_p.p.srb, rhiSampler, rhiTexture ? rhiTexture : renderer.m_emptyTexture);
    }

    if (rhiTexture)
    {
      // Process the audio data
      if(audio.fft)
      {
        if(m_audioScratchpad.size() < audio.data.size())
          m_audioScratchpad.resize(audio.data.size() / 2);
        std::size_t bufferSize = audio.data.size() / audio.channels;
        std::size_t fftSize = bufferSize / 2;
        const float norm = 1. / (2. * bufferSize);
        for(int i = 0; i < audio.channels; i++)
        {
          float* inputData = audio.data.data() + i * bufferSize;
          auto spectrum = m_fft.execute(inputData, bufferSize);

          float* outputSpectrum = m_audioScratchpad.data() + i * fftSize;
          for(std::size_t k = 0; k < fftSize; k++)
          {
            outputSpectrum[k] = 0.5f + spectrum[k][0] * norm;
          }
        }

        // Copy it
        QRhiTextureSubresourceUploadDescription subdesc(m_audioScratchpad.data(), (audio.data.size() / 2) * sizeof(float));
        QRhiTextureUploadEntry entry{0, 0, subdesc};
        QRhiTextureUploadDescription desc{entry};
        res.uploadTexture(rhiTexture, desc);
      }
      else
      {
        if(m_audioScratchpad.size() < audio.data.size())
          m_audioScratchpad.resize(audio.data.size());
        for(std::size_t i = 0; i < audio.data.size(); i++) {
          m_audioScratchpad[i] = 0.5f + audio.data[i] / 2.f;
        }

        // Copy it
        QRhiTextureSubresourceUploadDescription subdesc(m_audioScratchpad.data(), audio.data.size() * sizeof(float));
        QRhiTextureUploadEntry entry{0, 0, subdesc};
        QRhiTextureUploadDescription desc{entry};
        res.uploadTexture(rhiTexture, desc);
      }
    }
  }

  ossia::fft m_fft;

  void update(Renderer& renderer, QRhiResourceUpdateBatch& res) override
  {
    if (m_materialUBO && m_materialSize > 0 && materialChangedIndex != n.materialChanged)
    {
      char* data = n.m_materialData.get();
      res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
      materialChangedIndex = n.materialChanged;
    }

    for (auto& audio : n.audio_textures)
    {
      updateAudioTexture(audio, renderer, res);
    }
    {
      // Update all the process UBOs
      for(int i = 0, N = m_passes.size(); i < N; i++)
      {
        n.standardUBO.passIndex = i;
        res.updateDynamicBuffer(m_passes[i].processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
      }
    }
  }

  void releaseWithoutRenderTarget(Renderer& r) override
  {
    // customRelease
    {
      for (auto& texture : n.audio_textures)
      {
        auto it = texture.samplers.find(&r);
        if(it != texture.samplers.end())
        {
          if (auto tex = it->second.second)
          {
            if (tex != r.m_emptyTexture)
              tex->releaseAndDestroyLater();
          }
        }
      }

      for (int i = 0; i < int(m_passes.size()) - 1; i++)
      {
        auto& pass = m_passes[i];
        // TODO do we also want to remove the last pass texture here ?!
        pass.p.release();
        pass.renderTarget.release();
        pass.processUBO->releaseAndDestroyLater();
      }
      if(!m_passes.empty())
      {
        delete m_passes.back().p.pipeline;
        delete m_passes.back().p.srb;
        delete m_passes.back().processUBO;
      }

      m_passes.clear();
    }

    for (auto sampler : m_samplers)
    {
      delete sampler.sampler;
      // texture isdeleted elsewxheree
    }
    m_samplers.clear();

    delete m_materialUBO;
    m_materialUBO = nullptr;

    m_meshBuffer = nullptr;
  }

  void release(Renderer& r) override
  {
    releaseWithoutRenderTarget(r);
    m_lastPassRT.release();
  }


  void runPass(Renderer& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch& res) override
  {
    // if(m_passes.empty())
    //   return RenderedNode::runPass(renderer, cb, res);

    // Update a first time everything

    // PASSINDEX must be set to the last index
    // FIXME
    n.standardUBO.passIndex = m_passes.size() - 1;

    update(renderer, res);

    auto updateBatch = &res;

    // Draw the passes
    for(const auto& pass : m_passes)
    {
      SCORE_ASSERT(pass.renderTarget.renderTarget);
      SCORE_ASSERT(pass.p.pipeline);
      SCORE_ASSERT(pass.p.srb);
      // TODO : combine all the uniforms..

      auto rt = pass.renderTarget.renderTarget;
      auto pipeline = pass.p.pipeline;
      auto srb = pass.p.srb;
      auto texture = pass.renderTarget.texture;

      // TODO need to free stuff
      cb.beginPass(rt, Qt::black, {1.0f, 0}, updateBatch);
      {
        cb.setGraphicsPipeline(pipeline);
        cb.setShaderResources(srb);

        if(texture)
        {
          cb.setViewport(QRhiViewport(0, 0, texture->pixelSize().width(), texture->pixelSize().height()));
        }
        else
        {
          const auto sz = renderer.state.size;
          cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
        }

        assert(this->m_meshBuffer);
        assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
        n.mesh().setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

        cb.draw(n.mesh().vertexCount);
      }
      cb.endPass();

      if(pass.p.pipeline != m_passes.back().p.pipeline)
      {
        // Not the last pass: we have to use another resource batch
        updateBatch = renderer.state.rhi->nextResourceUpdateBatch();
      }
    }
  }
};

score::gfx::NodeRenderer* ISFNode::createRenderer() const noexcept
{
  return new RenderedISFNode{*this};
}

RenderedISFNode::~RenderedISFNode()
{

}
