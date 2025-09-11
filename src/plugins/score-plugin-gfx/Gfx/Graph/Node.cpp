#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/gfx/port_index.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>

namespace score::gfx
{
Node::Node() { }

void Node::update() { }
Node::~Node()
{
  for(auto port : input)
    delete port;
  for(auto port : output)
    delete port;
}

void Node::renderedNodesChanged() { }
void Node::process(Message&& msg) { }

NodeModel::NodeModel() { }

NodeModel::~NodeModel() { }

score::gfx::NodeRenderer* NodeModel::createRenderer(RenderList& r) const noexcept
{
  return new GenericNodeRenderer{*this};
}

void ProcessNode::process(Timings tk)
{
  score::gfx::ProcessUBO& UBO = this->standardUBO;
  auto prev_time = UBO.time;

  UBO.time = tk.date.impl / ossia::flicks_per_second<double>;
  UBO.timeDelta = UBO.time - prev_time;

  if(tk.parent_duration.impl > 0)
    UBO.progress = tk.date.impl / double(tk.parent_duration.impl);
  else
    UBO.progress = 0.;

  UBO.passIndex = 0;
}

void ProcessNode::process(int32_t port, const ossia::value& v)
{
  using namespace score::gfx;
  struct vec_visitor
  {
    const std::vector<ossia::value>& v;
    void operator()(ossia::monostate) const noexcept { }
    void operator()(float& val) const noexcept
    {
      if(!v.empty())
        val = ossia::convert<float>(v[0]);
    }
    void operator()(ossia::vec2f& val) const noexcept
    {
      val = ossia::convert<ossia::vec2f>(v);
    }
    void operator()(ossia::vec3f& val) const noexcept
    {
      val = ossia::convert<ossia::vec3f>(v);
    }
    void operator()(ossia::vec4f& val) const noexcept
    {
      val = ossia::convert<ossia::vec4f>(v);
    }
    void operator()(image& val) const noexcept { }
  };

  struct value_visitor
  {
    Types type{};
    void* value{};
    void operator()() const noexcept { }
    void operator()(ossia::impulse) const noexcept { }
    void operator()(int v) const noexcept
    {
      switch(type)
      {
        case Types::Int:
          memcpy(value, &v, 4);
          break;
        case Types::Float: {
          float fv = v;
          memcpy(value, &fv, 4);
          break;
        }
        default:
          break;
      }
    }
    void operator()(float v) const noexcept
    {
      switch(type)
      {
        case Types::Int: {
          int iv = v;
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          memcpy(value, &v, 4);
          break;
        }
        default:
          break;
      }
    }
    void operator()(bool v) const noexcept
    {
      switch(type)
      {
        case Types::Int: {
          int iv = v;
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          float fv = v;
          memcpy(value, &fv, 4);
          break;
        }
        default:
          break;
      }
    }
    void operator()(char v) const noexcept
    {
      switch(type)
      {
        case Types::Int: {
          int iv = v;
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          float fv = v;
          memcpy(value, &fv, 4);
          break;
        }
        default:
          break;
      }
    }
    void operator()(const std::string& v) const noexcept { }
    void operator()(ossia::vec2f v) const noexcept
    {
      switch(type)
      {
        case Types::Int: {
          int iv = v[0];
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          float fv = v[0];
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2: {
          memcpy(value, v.data(), 8);
          break;
        }
        case Types::Vec3: {
          memcpy(value, v.data(), 8);
          *(reinterpret_cast<float*>(value) + 2) = 0.f;
          break;
        }
        case Types::Vec4: {
          memcpy(value, v.data(), 8);
          *(reinterpret_cast<float*>(value) + 2) = 0.f;
          *(reinterpret_cast<float*>(value) + 3) = 0.f;
          break;
        }
        default:
          break;
      }
    }

    void operator()(ossia::vec3f v) const noexcept
    {
      switch(type)
      {
        case Types::Int: {
          int iv = v[0];
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          float fv = v[0];
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2: {
          memcpy(value, v.data(), 8);
          break;
        }
        case Types::Vec3: {
          memcpy(value, v.data(), 12);
          break;
        }
        case Types::Vec4: {
          memcpy(value, v.data(), 12);
          *(reinterpret_cast<float*>(value) + 3) = 0.f;
          break;
        }
        default:
          break;
      }
    }
    void operator()(ossia::vec4f v) const noexcept
    {
      switch(type)
      {
        case Types::Int: {
          int iv = v[0];
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          float fv = v[0];
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2: {
          memcpy(value, v.data(), 8);
          break;
        }
        case Types::Vec3: {
          memcpy(value, v.data(), 12);
          break;
        }
        case Types::Vec4: {
          memcpy(value, v.data(), 16);
          break;
        }
        default:
          break;
      }
    }
    void operator()(const std::vector<ossia::value>& v) const noexcept
    {
      if(v.empty())
        return;

      switch(type)
      {
        case Types::Int: {
          int iv = ossia::convert<int>(v[0]);
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float: {
          float fv = ossia::convert<float>(v[0]);
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2: {
          (*this)(ossia::convert<ossia::vec2f>(v));
          break;
        }
        case Types::Vec3: {
          (*this)(ossia::convert<ossia::vec3f>(v));
          break;
        }
        case Types::Vec4: {
          (*this)(ossia::convert<ossia::vec4f>(v));
          break;
        }
        default:
          break;
      }
    }

    void operator()(const ossia::value_map_type& v) const noexcept { }
  };

  if(int(this->input.size()) > port)
  {
    auto& in = this->input[port];
    v.apply(value_visitor{in->type, in->value});
    this->materialChange();
  }
  else
  {
    qDebug() << (typeid(*this).name()) << port << " > " << this->input.size();
  }
}

void ProcessNode::process(int32_t port, const ossia::audio_vector& v)
{
  if(v.empty() || v[0].empty())
    return;

  if(int(this->input.size()) <= port)
  {
    qDebug() << (typeid(*this).name()) << port << " > " << this->input.size();
    return;
  }
  auto& in = this->input[port];
  assert(in->type == score::gfx::Types::Audio);
  score::gfx::AudioTexture& tex = *(score::gfx::AudioTexture*)in->value;

  tex.channels = v.size();
  tex.data.clear();
  // if(tex.fixedSize)
  {
      // TODO

  } // else
  {
    tex.data.resize(v.size() * v[0].size());

    float* sample = tex.data.data();
    for(auto& chan : v)
    {
      for(int i = 0, N = chan.size(); i < N; ++i, ++sample)
        *sample = chan[i];
    }
  }
}

void Node::process(int32_t port, const ossia::render_target_spec& v)
{
  auto it = this->renderTargetSpecs.find(port);
  if(it != this->renderTargetSpecs.end())
  {
    if(it->second != v)
    {
      it->second = v;
      renderTargetChange();
    }
  }
  else
  {
    renderTargetSpecs.emplace(port, v);
    renderTargetChange();
  }
}

void ProcessNode::process(int32_t port, const ossia::geometry_spec& v)
{
  if(this->geometry != v || this->geometryChanged == 0)
  {
    this->geometry = v;
    ++this->geometryChanged;
  }
}

void ProcessNode::process(int32_t port, const ossia::transform3d& v) { }

QSize Node::resolveRenderTargetSize(int32_t port, RenderList& renderer) const noexcept
{
  auto it = this->renderTargetSpecs.find(port);
  if(it != this->renderTargetSpecs.end())
  {
    if(auto& sz = it->second.size)
    {
      return QSize{sz->width, sz->height};
    }
  }
  return renderer.state.renderSize;
}

static constexpr QRhiTexture::Format ossia_format_to_rhi(ossia::texture_format fmt)
{
  switch(fmt)
  {
    default:
    case ossia::texture_format::RGBA8:
      return QRhiTexture::Format::RGBA8;
    case ossia::texture_format::BGRA8:
      return QRhiTexture::Format::BGRA8;
    case ossia::texture_format::R8:
      return QRhiTexture::Format::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case ossia::texture_format::RG8:
      return QRhiTexture::Format::RG8;
#endif

    case ossia::texture_format::R16:
      return QRhiTexture::Format::R16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case ossia::texture_format::RG16:
      return QRhiTexture::Format::RG16;
#endif

    case ossia::texture_format::RED_OR_ALPHA8:
      return QRhiTexture::Format::RED_OR_ALPHA8;

    case ossia::texture_format::RGBA16F:
      return QRhiTexture::Format::RGBA16F;
    case ossia::texture_format::RGBA32F:
      return QRhiTexture::Format::RGBA32F;
    case ossia::texture_format::R16F:
      return QRhiTexture::Format::R16F;
    case ossia::texture_format::R32F:
      return QRhiTexture::Format::R32F;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case ossia::texture_format::RGB10A2:
      return QRhiTexture::Format::RGB10A2;
#endif

    case ossia::texture_format::D16:
      return QRhiTexture::Format::D16;
    case ossia::texture_format::D24:
      return QRhiTexture::Format::D24;
    case ossia::texture_format::D24S8:
      return QRhiTexture::Format::D24S8;
    case ossia::texture_format::D32F:
      return QRhiTexture::Format::D32F;
  }
}

RenderTargetSpecs
Node::resolveRenderTargetSpecs(int32_t port, RenderList& renderer) const noexcept
{
  RenderTargetSpecs spec;
  auto it = this->renderTargetSpecs.find(port);
  if(it != this->renderTargetSpecs.end())
  {
    if(it->second.size)
      spec.size = QSize{it->second.size->width, it->second.size->height};
    spec.format = ossia_format_to_rhi(it->second.format);
    spec.address_u = static_cast<decltype(spec.address_u)>(it->second.address_u);
    spec.address_v = static_cast<decltype(spec.address_v)>(it->second.address_v);
    spec.address_w = static_cast<decltype(spec.address_w)>(it->second.address_w);
    spec.mag_filter = static_cast<decltype(spec.mag_filter)>(it->second.mag_filter);
    spec.min_filter = static_cast<decltype(spec.min_filter)>(it->second.min_filter);
    spec.mipmap_mode = static_cast<decltype(spec.mag_filter)>(it->second.mipmap_mode);
  }

  if(spec.size == QSize{})
    spec.size = {renderer.state.renderSize.width(), renderer.state.renderSize.height()};

  return spec;
}

void ProcessNode::process(
    int32_t port, const std::function<void(score::gfx::Node&)>& func)
{
  SCORE_ASSERT(func);
  func(*this);
}

void ProcessNode::process(Message&& msg)
{
  process(msg.token);

  int32_t p = 0;
  for(const gfx_input& m : msg.input)
  {
    auto sink = ossia::gfx::port_index{msg.node_id, p};
    ossia::visit(
        [this, sink](const auto& v) { this->process(sink.port, v); }, std::move(m));

    p++;
  }
}
}

QDebug operator<<(QDebug s, const score::gfx::gfx_input& v)
{
  struct
  {
    QDebug& s;
    void operator()(ossia::monostate) { s << "monostate"; }
    void operator()(const std::function<void(score::gfx::Node&)>) { s << "function"; }
    void operator()(const ossia::value& v)
    {
      s << "value:" << QByteArray::fromStdString(ossia::value_to_pretty_string(v));
    }
    void operator()(const ossia::audio_vector& v)
    {
      s << "audio: " << v.size() << "channels";
    }
    void operator()(const ossia::render_target_spec& v) { s << "texture"; }
    void operator()(const ossia::geometry_spec& v) { s << "meshlist"; }
    void operator()(const ossia::transform3d& v) { s << "transform3d"; }
  } print{s};
  ossia::visit(print, std::move(v));
  return s;
}
QDebug operator<<(QDebug s, const std::vector<score::gfx::gfx_input>& v)
{
  s << "[";
  for(const auto& m : v)
  {
    s << m << " ; ";
  }
  s << "]";
  return s;
}
