#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/RenderedCSFNode.hpp>
#include <Gfx/Graph/RenderedISFNode.hpp>
#include <Gfx/Graph/RenderedRawRasterPipelineNode.hpp>
#include <Gfx/Graph/RenderedVSANode.hpp>
#include <Gfx/Graph/SimpleRenderedISFNode.hpp>

#include <score/tools/Debug.hpp>

#include <boost/algorithm/string/replace.hpp>

#include <algorithm>

namespace score::gfx
{
struct isf_input_port_vis
{
  ISFNode& self;
  char* data{};
  int sz{};

  // Initial values are the UI controls' (ISFProcess.hpp setupISFModelPorts),
  // so that a preview and the first frame show what the controls do.
  void operator()(const isf::float_input& in) noexcept
  {
    *reinterpret_cast<float*>(data) = in.def;
    self.input.push_back(new Port{&self, data, Types::Float, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::long_input& in) noexcept
  {
    // Enum mode (VALUES/LABELS set): in.def is the *index* into VALUES, but
    // the shader and the UI pipeline downstream consume the numeric VALUE at
    // that index. Look it up here so the initial UBO state matches what the
    // ComboBox emits after any user interaction. String-valued VALUES fall
    // back to the index (GLSL can't receive strings).
    int initial = (int)in.def;
    if(!in.values.empty())
    {
      auto idx = std::min<std::size_t>(in.def, in.values.size() - 1);
      const auto& v = in.values[idx];
      if(auto i = ossia::get_if<int64_t>(&v))
        initial = (int)*i;
      else if(auto d = ossia::get_if<double>(&v))
        initial = (int)*d;
      else
        initial = (int)idx;
    }
    *reinterpret_cast<int*>(data) = initial;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    data += 4;
    sz += 4;
  }

  void operator()(const isf::event_input& in) noexcept
  {
    *reinterpret_cast<int*>(data) = 0;
    self.input.push_back(new Port{&self, data, Types::Int, {}});
    self.m_event_ports.push_back(reinterpret_cast<int*>(data));
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
    if(sz % 8 != 0)
    {
      sz += 4;
      data += 4;
    }

    const auto& arr = in.def.value_or(std::array<double, 2>{0., 0.});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    self.input.push_back(new Port{&self, data, Types::Vec2, {}});
    data += 2 * 4;
    sz += 2 * 4;
  }

  void operator()(const isf::point3d_input& in) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }
    const auto& arr = in.def.value_or(std::array<double, 3>{0., 0., 0.});
    *reinterpret_cast<float*>(data) = arr[0];
    *reinterpret_cast<float*>(data + 4) = arr[1];
    *reinterpret_cast<float*>(data + 8) = arr[2];
    self.input.push_back(new Port{&self, data, Types::Vec3, {}});
    data += 3 * 4;
    sz += 3 * 4;
  }

  void operator()(const isf::color_input& in) noexcept
  {
    while(sz % 16 != 0)
    {
      sz += 4;
      data += 4;
    }
    const auto& arr = in.def.value_or(std::array<double, 4>{0.5, 0.5, 0.5, 1.});
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
    // GrabsFromSource = "fetch the QRhiTexture* straight from the upstream
    // renderer's textureForOutput() instead of allocating our own render
    // target". Required for:
    //  - 3D textures (volumes): no render-target path exists for them.
    //  - Texture arrays: consumers (e.g. classic_pbr_textured sampling a
    //    per-material base_color_array from ScenePreprocessor) need the
    //    producer's actual QRhiTexture array, not an empty render-target
    //    texture created on their side.
    //  - "STATIC: true" image inputs (shader-author opt-in): the upstream
    //    is a CPU producer that publishes a long-lived QRhiTexture
    //    (precomputed LUTs, IBL bakes, asset caches). Without this opt-in
    //    the consumer would silently allocate an unused render target and
    //    bind that empty texture instead of the producer's real one,
    //    making the input read all zeros.
    auto flags = (in.dimensions == 3 || in.is_array || in.is_static)
                     ? Flag::GrabsFromSource
                     : Flag{};
    if(in.depth)
      flags = flags | Flag::SamplableDepth;
    if(in.is_array)
      flags = flags | Flag::TextureArray;
    if(in.dimensions == 3)
      flags = flags | Flag::ThreeDimensional;
    self.input.push_back(new Port{&self, {}, Types::Image, flags, {}});
  }

  void operator()(const isf::cubemap_input& in) noexcept
  {
    auto flags = Flag::GrabsFromSource | Flag::Cubemap;
    if(in.depth)
      flags = flags | Flag::SamplableDepth;
    self.input.push_back(new Port{&self, {}, Types::Image, flags, {}});
  }

  void operator()(const isf::audio_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    data.filter = audio.sampler.filter;
    data.wrap = audio.sampler.wrap;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  void operator()(const isf::audioHist_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    data.mode = data.Histogram;
    data.filter = audio.sampler.filter;
    data.wrap = audio.sampler.wrap;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  void operator()(const isf::audioFFT_input& audio) noexcept
  {
    self.m_audio_textures.push_back({});
    auto& data = self.m_audio_textures.back();
    data.fixedSize = audio.max;
    data.mode = AudioTexture::Mode::FFT;
    data.filter = audio.sampler.filter;
    data.wrap = audio.sampler.wrap;
    self.input.push_back(new Port{&self, &data, Types::Audio, {}});
  }

  // CSF-specific input handlers
  void operator()(const isf::storage_input& in) noexcept
  {
    // According to the spec:
    // - read_only: input port
    // - write_only: output port
    // - read_write: output port only, buffer is persistent
    //
    // BUFFER_USAGE="indirect_draw[_indexed]": port additionally carries the
    // IndirectDraw flag so renderers can route it to the indirect-draw
    // mechanism on MeshBuffers.

    auto extra_flags = Flag{};
    if(in.buffer_usage == "indirect_draw" || in.buffer_usage == "indirect_draw_indexed")
      extra_flags = extra_flags | Flag::IndirectDraw;

    if(in.access == "read_only")
    {
      // Create input port for read-only storage buffer
      self.input.push_back(new Port{&self, {}, Types::Buffer, extra_flags, {}});
    }
    else if(in.access.contains("write"))
    {
      // Create output port for write-only storage buffer
      self.output.push_back(new Port{&self, {}, Types::Buffer, extra_flags, {}});

      // Check for flexible array member
      if(!in.layout.empty())
      {
        const auto& lastField = in.layout.back();
        if(lastField.type.find("[]") != std::string::npos)
        {
          (*this)(isf::long_input{.values = {}, .labels = {}, .def = 1024});
        }
      }
    }
  }

  void operator()(const isf::uniform_input& in) noexcept
  {
    // Read-only UBO sourced from upstream Buffer port. Renderers bind it via
    // QRhiShaderResourceBinding::uniformBuffer (not bufferLoad).
    self.input.push_back(new Port{&self, {}, Types::Buffer, Flag::UniformBuffer, {}});
  }

  void operator()(const isf::texture_input& in) noexcept
  {
    auto flags = in.dimensions == 3
                     ? (Flag::GrabsFromSource | Flag::ThreeDimensional)
                     : Flag{};
    self.input.push_back(new Port{&self, {}, Types::Image, flags, {}});
  }

  void operator()(const isf::geometry_input& in) noexcept
  {
    if(self.m_descriptor.mode != isf::descriptor::RawRaster)
    {
      if(in.attributes.empty())
      {
        // No attributes declared: pure pass-through (input + output, forwards everything)
        self.input.push_back(new Port{&self, {}, Types::Geometry, {}});
        self.output.push_back(new Port{&self, {}, Types::Geometry, {}});
      }
      else
      {
        // Create geometry input port if any attribute needs upstream data (read_only or read_write)
        bool needs_input = false;
        for(const auto& attr : in.attributes)
          if(attr.access == "read_only" || attr.access == "read_write")
          { needs_input = true; break; }

        if(needs_input)
          self.input.push_back(new Port{&self, {}, Types::Geometry, {}});

        // If any attributes are writable, create a geometry output port
        bool has_output = false;
        for(const auto& attr : in.attributes)
        {
          if(attr.access == "write_only" || attr.access == "read_write")
          {
            has_output = true;
            break;
          }
        }
        if(has_output)
        {
          self.output.push_back(new Port{&self, {}, Types::Geometry, {}});
        }
      }
    }

    // $USER in vertex_count/instance_count/aux.size → IntSpinBox port
    if(in.vertex_count.find("$USER") != std::string::npos)
      (*this)(isf::long_input{.values = {}, .labels = {}, .def = 1024});
    if(in.instance_count.find("$USER") != std::string::npos)
      (*this)(isf::long_input{.values = {}, .labels = {}, .def = 1});
    for(const auto& aux : in.auxiliary)
      if(aux.size.find("$USER") != std::string::npos)
        (*this)(isf::long_input{.values = {}, .labels = {}, .def = 1024});
  }

  void operator()(const isf::csf_image_input& in) noexcept
  {
    // CSF image inputs - these can be read-only, write-only, or read-write
    if(in.access == "read_only")
    {
      // Input port for read-only image; 3D textures use GrabsFromSource
      auto flags = in.is3D()
                       ? (Flag::GrabsFromSource | Flag::ThreeDimensional)
                       : Flag{};
      self.input.push_back(new Port{&self, {}, Types::Image, flags, {}});
    }
    else if(in.access == "write_only" || in.access == "read_write")
    {
      // Output port for the image
      self.output.push_back(new Port{&self, {}, Types::Image, {}});
      // FIXME:
      // (*this)(isf::point2d_input{ ... });
      // (*this)(isf::long_input{ ... });
    }
  }
};

namespace
{
//! Whether a fragment shader assigns gl_FragDepth.
//!
//! Metal refuses a pipeline whose fragment shader writes depth against a
//! render pass with no depth attachment; GL and Vulkan accept it and drop the
//! write. The depth-forwarding filters -- a post effect that reads a depth
//! input, colours from it and re-emits `gl_FragDepth = rawD` so the next pass
//! still has depth -- need the attachment on every backend anyway.
bool writesFragDepth(QStringView frag) noexcept
{
  constexpr QLatin1StringView name("gl_FragDepth");
  for(qsizetype i = frag.indexOf(name); i >= 0; i = frag.indexOf(name, i + 1))
  {
    qsizetype j = i + name.size();
    while(j < frag.size() && frag[j].isSpace())
      ++j;
    if(j >= frag.size())
      break;

    // `gl_FragDepth =` writes, `gl_FragDepth ==` compares.
    if(frag[j] == u'=')
    {
      if(j + 1 >= frag.size() || frag[j + 1] != u'=')
        return true;
      continue;
    }

    // The compound forms write too.
    if(QStringView(u"+-*/").contains(frag[j]) && j + 1 < frag.size()
       && frag[j + 1] == u'=')
      return true;
  }
  return false;
}
}

struct ISFNode::SizeExpressions
{
  std::string width, height;
  // The variables both expressions read, by reference: sized once, before
  // they are registered, so that their addresses never move.
  std::vector<double> values;
  ossia::math_expression widthExpr, heightExpr;
  bool widthOk{}, heightOk{};
};

ISFNode::ISFNode(const isf::descriptor& desc, const QString& vert, const QString& frag)
    : m_descriptor{desc}
{
  m_vertexS = vert;
  m_fragmentS = frag;

  // Compoute the size required for the materials
  isf_input_size_vis sz_vis{};

  if(qEnvironmentVariableIsSet("SCORE_GFX_TRACE"))
    fprintf(
        stderr, "GFX-ISFNODE ctor mode=%d (RawRaster=%d) inputs=%zu\n",
        (int)desc.mode, (int)isf::descriptor::RawRaster, desc.inputs.size());

  if(desc.mode == isf::descriptor::RawRaster)
  {
    // Geometry port input
    input.push_back(new Port{this, {}, Types::Geometry, {}});
    this->requiresDepth = true;
  }

  if(writesFragDepth(frag))
    this->requiresDepth = true;

  // Size of the inputs
  for(const isf::input& input : desc.inputs)
  {
    ossia::visit(sz_vis, input.data);
  }

  m_materialSize = sz_vis.sz;
  m_materialUBOSize = sz_vis.sz;

  // Allocate the required memory
  // TODO : this must be per-renderer, as the texture sizes may depend on the renderer....
  m_material_data.reset(new char[m_materialSize]);
  std::fill_n(m_material_data.get(), m_materialSize, 0);
  char* cur = m_material_data.get();

  // Create ports pointing to the data used for the UBO
  isf_input_port_vis visitor{*this, cur};
  for(const isf::input& input : desc.inputs)
    ossia::visit(visitor, input.data);

  if(hasCameraInput(desc))
  {
    m_cameraInput = (int)input.size();
    input.push_back(new Port{this, {}, Types::Scene, {}});
  }

  if(desc.outputs.empty())
  {
    // Default: single color output
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
  else
  {
    // MRT: one output port per declared output (color or depth)
    for(const auto& out : desc.outputs)
    {
      output.push_back(new Port{this, {}, Types::Image, {}});
      if(out.type == "depth")
        this->requiresDepth = true;
    }
  }
}

ISFNode::ISFNode(const isf::descriptor& desc, const QString& comp)
    : m_descriptor{desc}
{
  m_computeS = comp;

  // Compute the size required for the materials
  isf_input_size_vis sz_vis{};

  // Size of the inputs
  for(const isf::input& input : desc.inputs)
  {
    ossia::visit(sz_vis, input.data);
  }

  // The shader's material UBO covers the declared inputs and nothing else.
  m_materialUBOSize = sz_vis.sz;

  // Add space for USER dispatch ports (3 ints per USER dispatch pass)
  int user_dispatch_count = 0;
  for(const auto& pass : desc.csf_passes)
    if(pass.execution_type == "USER")
      user_dispatch_count++;
  sz_vis.sz += user_dispatch_count * 3 * 4;

  m_materialSize = sz_vis.sz;

  // Allocate the required memory
  // TODO : this must be per-renderer, as the texture sizes may depend on the renderer....
  m_material_data.reset(new char[m_materialSize]);
  std::fill_n(m_material_data.get(), m_materialSize, 0);
  char* cur = m_material_data.get();

  // Create ports pointing to the data used for the UBO
  isf_input_port_vis visitor{*this, cur};
  for(const isf::input& input : desc.inputs)
    ossia::visit(visitor, input.data);

  // Create USER dispatch ports (XYZ IntSpinBox per USER dispatch pass)
  for(auto& pass : m_descriptor.csf_passes)
  {
    if(pass.execution_type == "USER")
    {
      for(int axis = 0; axis < 3; axis++)
      {
        pass.user_dispatch_ports[axis] = (int)this->input.size();
        visitor(isf::long_input{.values = {}, .labels = {}, .def = std::size_t(1)});
      }
    }
  }

  // For CSF, if no output ports were created by the inputs, create a default one
  if(desc.mode == isf::descriptor::CSF && output.empty())
  {
    output.push_back(new Port{this, {}, Types::Image, {}});
  }
}

ISFNode::~ISFNode() { }

bool ISFNode::hasCameraInput(const isf::descriptor& desc) noexcept
{
  return desc.mode == isf::descriptor::RawRaster
         && std::any_of(desc.auxiliary.begin(), desc.auxiliary.end(), [](const auto& aux) {
    return aux.name == "camera" && aux.is_uniform;
  });
}

void ISFNode::process(Message&& msg)
{
  // Normal processing
  ProcessNode::process(std::move(msg));
}

std::atomic<uint64_t>& sizeExpressionCompilations() noexcept
{
  static std::atomic<uint64_t> n{0};
  return n;
}

QSize ISFNode::computeTextureSize(
    const isf::pass& pass, QSize origSize, std::span<const Sampler> inputSamplers)
{
  QSize res = origSize;
  if(pass.width_expression.empty() && pass.height_expression.empty())
    return res;

  // $WIDTH / $HEIGHT, then per input: an image's size, a float's or long's
  // value. Inputs create zero (a written buffer or image), one, or several
  // ($USER sizes) input ports: walk them with the canonical counts, not one
  // port each. The same walk names the variables and refreshes their values.
  auto collect = [&](std::vector<double>& values, std::vector<std::string>* names) {
    values.clear();
    auto add = [&](std::string name, double v) {
      values.push_back(v);
      if(names)
        names->push_back(std::move(name));
    };
    add("var_WIDTH", res.width());
    add("var_HEIGHT", res.height());
    walk_descriptor_inputs(
        m_descriptor, [&](const isf::input& input, port_counts cur, port_counts delta) {
      if(delta.inlets == 0 || cur.inlets >= (int)this->input.size())
        return;
      auto port = this->input[cur.inlets];
      if(ossia::get_if<isf::image_input>(&input.data)
         || ossia::get_if<isf::cubemap_input>(&input.data))
      {
        QSize sz = origSize;
        if(cur.samplers < (int)inputSamplers.size() && inputSamplers[cur.samplers].texture)
          sz = inputSamplers[cur.samplers].texture->pixelSize();
        add("var_WIDTH_" + input.name, sz.width());
        add("var_HEIGHT_" + input.name, sz.height());
      }
      else if(ossia::get_if<isf::float_input>(&input.data))
      {
        add("var_" + input.name, *(float*)port->value);
      }
      else if(ossia::get_if<isf::long_input>(&input.data))
      {
        add("var_" + input.name, *(int*)port->value);
      }
    });
  };

  std::lock_guard lock{m_sizeExpressionsMutex};
  SizeExpressions* e{};
  for(auto& cached : m_sizeExpressions)
    if(cached->width == pass.width_expression && cached->height == pass.height_expression)
      e = cached.get();

  std::vector<double> current;
  if(e)
  {
    collect(current, nullptr);
    if(current.size() != e->values.size())
      e = nullptr; // the inputs changed shape: compile again
    else
      std::copy(current.begin(), current.end(), e->values.begin());
  }
  if(!e)
  {
    std::erase_if(m_sizeExpressions, [&](const auto& cached) {
      return cached->width == pass.width_expression
             && cached->height == pass.height_expression;
    });
    auto fresh = std::make_unique<SizeExpressions>();
    fresh->width = pass.width_expression;
    fresh->height = pass.height_expression;
    std::vector<std::string> names;
    collect(fresh->values, &names);
    auto compile = [&](ossia::math_expression& expr, std::string text) {
      if(text.empty())
        return false;
      for(std::size_t i = 0; i < names.size(); i++)
        expr.add_variable(names[i], fresh->values[i]);
      expr.register_symbol_table();
      boost::algorithm::replace_all(text, "$", "var_");
      sizeExpressionCompilations()++;
      if(!expr.set_expression(text))
      {
        qDebug() << expr.error().c_str() << text.c_str();
        return false;
      }
      return true;
    };
    fresh->widthOk = compile(fresh->widthExpr, fresh->width);
    fresh->heightOk = compile(fresh->heightExpr, fresh->height);
    e = fresh.get();
    m_sizeExpressions.push_back(std::move(fresh));
  }

  if(e->widthOk)
    if(const double v = e->widthExpr.value(); is_texture_dimension(v))
      res.setWidth(v);
  if(e->heightOk)
    if(const double v = e->heightExpr.value(); is_texture_dimension(v))
      res.setHeight(v);
  return res;
}

score::gfx::NodeRenderer* ISFNode::createRenderer(RenderList& r) const noexcept
{
  if(this->m_descriptor.passes.empty() && this->m_descriptor.csf_passes.empty())
  {
    return nullptr;
  }

  switch(this->m_descriptor.mode)
  {
    case isf::descriptor::VSA:
      return new SimpleRenderedVSANode{*this};

    case isf::descriptor::CSF:
      return new RenderedCSFNode{*this};

    case isf::descriptor::RawRaster:
      return new RenderedRawRasterPipelineNode{*this};

    case isf::descriptor::ISF:
      switch(this->m_descriptor.passes.size())
      {
        case 1: {
          // A pass that declares its own size, or a single-channel FORMAT,
          // needs its own target, which only the multipass renderer allocates
          // (shader board F04: the simple renderer drew into the destination
          // regardless). FLOAT, a four-channel FORMAT and FILTER alone do not:
          // one draw at the destination's size gives the same pixels into the
          // destination as into such a target blitted at 1:1. MRT OUTPUTS
          // stay on the simple renderer, which sizes them itself.
          const auto& pass = this->m_descriptor.passes[0];
          const auto singleChannel = [](std::string f) {
            for(auto& c : f)
              c = (char)std::tolower((unsigned char)c);
            return f == "r8" || f == "r16f" || f == "r32f";
          };
          const bool ownTarget
              = this->m_descriptor.outputs.empty()
                && (!pass.width_expression.empty() || !pass.height_expression.empty()
                    || singleChannel(pass.format));
          if(!pass.persistent && !ownTarget)
          {
            return new SimpleRenderedISFNode{*this};
          }
          else
          {
            return new RenderedISFNode{*this};
          }
          break;
        }
        default: {
          return new RenderedISFNode{*this};
          break;
        }
      }
      break;
  }
  return nullptr;
}
}

namespace score::gfx
{
void ISFNode::process(int32_t port, const ossia::value& v)
{
  if(!fireEventPort(port, v))
    ProcessNode::process(port, v);
}

void ISFNode::adjustRenderTargetSpecs(int32_t port, RenderTargetSpecs& spec) const noexcept
{
  const int first = m_descriptor.mode == isf::descriptor::RawRaster ? 1 : 0;
  const std::string* mipmap = nullptr;
  walk_descriptor_inputs(
      m_descriptor, port_counts{first, 0, 0},
      [&](const isf::input& inp, const port_counts& before, const port_counts& delta) {
    if(delta.inlets < 1 || before.inlets != port)
      return;
    if(auto* cfg = inputSamplerConfig(inp))
      mipmap = &cfg->mipmap_mode;
  });
  if(!mipmap || mipmap->empty())
    return;
  std::string m = *mipmap;
  for(auto& c : m)
    c = (char)std::tolower((unsigned char)c);
  if(m == "linear")
    spec.mipmap_mode = QRhiSampler::Linear;
  else if(m == "nearest")
    spec.mipmap_mode = QRhiSampler::Nearest;
}

}
