#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <ossia/network/value/value.hpp>
#include <ossia/network/value/value_conversion.hpp>
#include <ossia/gfx/port_index.hpp>

namespace score::gfx
{
Node::Node() { }

Node::~Node()
{
  for(auto port : input)
    delete port;
  for(auto port : output)
    delete port;
}

void Node::process(const Message& msg)
{

}

NodeModel::NodeModel() { }

NodeModel::~NodeModel() { }

score::gfx::NodeRenderer*
NodeModel::createRenderer(RenderList& r) const noexcept
{
  return new GenericNodeRenderer{*this};
}
/*
void NodeModel::setShaders(const QShader& vert, const QShader& frag)
{
  m_vertexS = vert;
  m_fragmentS = frag;
}
*/

void ProcessNode::process(const ossia::token_request& tk)
{
  score::gfx::ProcessUBO& UBO = this->standardUBO;
  auto prev_time = UBO.time;

  UBO.time = tk.date.impl / ossia::flicks_per_second<double>;
  UBO.timeDelta = UBO.time - prev_time;

  if (tk.parent_duration.impl > 0)
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
    void operator()(std::monostate) const noexcept { }
    void operator()(float& val) const noexcept
    {
      if (!v.empty())
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
      switch (type)
      {
        case Types::Int:
          memcpy(value, &v, 4);
          break;
        case Types::Float:
        {
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
      switch (type)
      {
        case Types::Int:
        {
          int iv = v;
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
          memcpy(value, &v, 4);
          break;
        }
        default:
          break;
      }
    }
    void operator()(bool v) const noexcept
    {
      switch (type)
      {
        case Types::Int:
        {
          int iv = v;
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
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
      switch (type)
      {
        case Types::Int:
        {
          int iv = v;
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
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
      switch (type)
      {
        case Types::Int:
        {
          int iv = v[0];
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
          float fv = v[0];
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2:
        {
          memcpy(value, v.data(), 8);
          break;
        }
        case Types::Vec3:
        {
          memcpy(value, v.data(), 8);
          *(reinterpret_cast<float*>(value) + 2) = 0.f;
          break;
        }
        case Types::Vec4:
        {
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
      switch (type)
      {
        case Types::Int:
        {
          int iv = v[0];
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
          float fv = v[0];
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2:
        {
          memcpy(value, v.data(), 8);
          break;
        }
        case Types::Vec3:
        {
          memcpy(value, v.data(), 12);
          break;
        }
        case Types::Vec4:
        {
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
      switch (type)
      {
        case Types::Int:
        {
          int iv = v[0];
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
          float fv = v[0];
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2:
        {
          memcpy(value, v.data(), 8);
          break;
        }
        case Types::Vec3:
        {
          memcpy(value, v.data(), 12);
          break;
        }
        case Types::Vec4:
        {
          memcpy(value, v.data(), 16);
          break;
        }
        default:
          break;
      }
    }
    void operator()(const std::vector<ossia::value>& v) const noexcept
    {
      if (v.empty())
        return;

      switch (type)
      {
        case Types::Int:
        {
          int iv = ossia::convert<int>(v[0]);
          memcpy(value, &iv, 4);
          break;
        }
        case Types::Float:
        {
          float fv = ossia::convert<float>(v[0]);
          memcpy(value, &fv, 4);
          break;
        }
        case Types::Vec2:
        {
          (*this)(ossia::convert<ossia::vec2f>(v));
          break;
        }
        case Types::Vec3:
        {
          (*this)(ossia::convert<ossia::vec3f>(v));
          break;
        }
        case Types::Vec4:
        {
          (*this)(ossia::convert<ossia::vec4f>(v));
          break;
        }
        default:
          break;
      }
    }
  };

  assert(int(this->input.size()) > port);

  auto& in = this->input[port];
  v.apply(value_visitor{in->type, in->value});
  this->materialChanged++;
}

void ProcessNode::process(int32_t port, const ossia::audio_vector& v)
{
  if (v.empty() || v[0].empty())
    return;

  assert(int(this->input.size()) > port);
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
    for (auto& chan : v)
    {
      for (int i = 0, N = chan.size(); i < N; ++i, ++sample)
        *sample = chan[i];
    }
  }
}
void ProcessNode::process(const Message& msg)
{
  process(msg.token);

  int32_t p = 0;
  for (const gfx_input& m : msg.input)
  {
    auto sink = ossia::gfx::port_index{msg.node_id, p};
    std::visit([this, sink] (const auto& v) { this->process(sink.port, v); }, std::move(m));

    p++;
  }
}

}
