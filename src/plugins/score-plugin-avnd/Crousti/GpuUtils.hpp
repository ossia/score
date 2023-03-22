#pragma once

#if SCORE_PLUGIN_GFX
#include <Process/ExecutionContext.hpp>

#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <QTimer>

#include <avnd/binding/ossia/port_run_postprocess.hpp>
#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <avnd/concepts/parameter.hpp>
#include <fmt/format.h>
#include <gpp/layout.hpp>

namespace gpp::qrhi
{
template <typename F>
  requires requires { F::format(); }
constexpr QRhiTexture::Format textureFormat()
{
  constexpr std::string_view fmt = F::format();

  if(fmt == "rgba" || fmt == "rgba8")
    return QRhiTexture::RGBA8;
  else if(fmt == "bgra" || fmt == "bgra8")
    return QRhiTexture::BGRA8;
  else if(fmt == "r8")
    return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "rg8")
    return QRhiTexture::RG8;
#endif
  else if(fmt == "r16")
    return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "rg16")
    return QRhiTexture::RG16;
#endif
  else if(fmt == "red_or_alpha8")
    return QRhiTexture::RED_OR_ALPHA8;
  else if(fmt == "rgba16f")
    return QRhiTexture::RGBA16F;
  else if(fmt == "rgba32f")
    return QRhiTexture::RGBA32F;
  else if(fmt == "r16f")
    return QRhiTexture::R16F;
  else if(fmt == "r32")
    return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "rgb10a2")
    return QRhiTexture::RGB10A2;
#endif

  else if(fmt == "d16")
    return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if(fmt == "d24")
    return QRhiTexture::D24;
  else if(fmt == "d24s8")
    return QRhiTexture::D24S8;
#endif
  else if(fmt == "d32f")
    return QRhiTexture::D32F;

  else if(fmt == "bc1")
    return QRhiTexture::BC1;
  else if(fmt == "bc2")
    return QRhiTexture::BC2;
  else if(fmt == "bc3")
    return QRhiTexture::BC3;
  else if(fmt == "bc4")
    return QRhiTexture::BC4;
  else if(fmt == "bc5")
    return QRhiTexture::BC5;
  else if(fmt == "bc6h")
    return QRhiTexture::BC6H;
  else if(fmt == "bc7")
    return QRhiTexture::BC7;
  else if(fmt == "etc2_rgb8")
    return QRhiTexture::ETC2_RGB8;
  else if(fmt == "etc2_rgb8a1")
    return QRhiTexture::ETC2_RGB8A1;
  else if(fmt == "etc2_rgb8a8")
    return QRhiTexture::ETC2_RGBA8;
  else if(fmt == "astc_4x4")
    return QRhiTexture::ASTC_4x4;
  else if(fmt == "astc_5x4")
    return QRhiTexture::ASTC_5x4;
  else if(fmt == "astc_5x5")
    return QRhiTexture::ASTC_5x5;
  else if(fmt == "astc_6x5")
    return QRhiTexture::ASTC_6x5;
  else if(fmt == "astc_6x6")
    return QRhiTexture::ASTC_6x6;
  else if(fmt == "astc_8x5")
    return QRhiTexture::ASTC_8x5;
  else if(fmt == "astc_8x6")
    return QRhiTexture::ASTC_8x6;
  else if(fmt == "astc_8x8")
    return QRhiTexture::ASTC_8x8;
  else if(fmt == "astc_10x5")
    return QRhiTexture::ASTC_10x5;
  else if(fmt == "astc_10x6")
    return QRhiTexture::ASTC_10x6;
  else if(fmt == "astc_10x8")
    return QRhiTexture::ASTC_10x8;
  else if(fmt == "astc_10x10")
    return QRhiTexture::ASTC_10x10;
  else if(fmt == "astc_12x10")
    return QRhiTexture::ASTC_12x10;
  else if(fmt == "astc_12x12")
    return QRhiTexture::ASTC_12x12;
  else
    return QRhiTexture::RGBA8;
}

template <typename F>
  requires std::is_enum_v<typename F::format>
constexpr QRhiTexture::Format textureFormat()
{
  if constexpr(
      requires { F::RGBA; } || requires { F::RGBA8; })
    return QRhiTexture::RGBA8;
  else if constexpr(
      requires { F::BGRA; } || requires { F::BGRA8; })
    return QRhiTexture::BGRA8;
  else if constexpr(
      requires { F::R8; } || requires { F::GRAYSCALE; })
    return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if constexpr(requires { F::RG8; })
    return QRhiTexture::RG8;
#endif
  else if constexpr(requires { F::R16; })
    return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if constexpr(requires { F::RG16; })
    return QRhiTexture::RG8;
#endif
  else if constexpr(requires { F::RED_OR_ALPHA8; })
    return QRhiTexture::RED_OR_ALPHA8;
  else if constexpr(requires { F::RGBA16F; })
    return QRhiTexture::RGBA16F;
  else if constexpr(requires { F::RGBA32F; })
    return QRhiTexture::RGBA32F;
  else if constexpr(requires { F::R16F; })
    return QRhiTexture::R16F;
  else if constexpr(requires { F::R32F; })
    return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if constexpr(requires { F::RGB10A2; })
    return QRhiTexture::RGB10A2;
#endif
  else if constexpr(requires { F::D16; })
    return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  else if constexpr(requires { F::D24; })
    return QRhiTexture::D24;
  else if constexpr(requires { F::D24S8; })
    return QRhiTexture::D24S8;
#endif
  else if constexpr(requires { F::D32F; })
    return QRhiTexture::D32F;

  else if constexpr(requires { F::BC1; })
    return QRhiTexture::BC1;
  else if constexpr(requires { F::BC2; })
    return QRhiTexture::BC2;
  else if constexpr(requires { F::BC3; })
    return QRhiTexture::BC3;
  else if constexpr(requires { F::BC4; })
    return QRhiTexture::BC4;
  else if constexpr(requires { F::BC5; })
    return QRhiTexture::BC5;
  else if constexpr(requires { F::BC6H; })
    return QRhiTexture::BC6H;
  else if constexpr(requires { F::BC7; })
    return QRhiTexture::BC7;
  else if(requires { F::ETC2_RGB8; })
    return QRhiTexture::ETC2_RGB8;
  else if(requires { F::ETC2_RGB8A1; })
    return QRhiTexture::ETC2_RGB8A1;
  else if(requires { F::ETC2_RGB8A8; })
    return QRhiTexture::ETC2_RGBA8;
  else if(requires { F::ASTC_4X4; })
    return QRhiTexture::ASTC_4x4;
  else if(requires { F::ASTC_5X4; })
    return QRhiTexture::ASTC_5x4;
  else if(requires { F::ASTC_5X5; })
    return QRhiTexture::ASTC_5x5;
  else if(requires { F::ASTC_6X5; })
    return QRhiTexture::ASTC_6x5;
  else if(requires { F::ASTC_6X6; })
    return QRhiTexture::ASTC_6x6;
  else if(requires { F::ASTC_8X5; })
    return QRhiTexture::ASTC_8x5;
  else if(requires { F::ASTC_8X6; })
    return QRhiTexture::ASTC_8x6;
  else if(requires { F::ASTC_8X8; })
    return QRhiTexture::ASTC_8x8;
  else if(requires { F::ASTC_10X5; })
    return QRhiTexture::ASTC_10x5;
  else if(requires { F::ASTC_10X6; })
    return QRhiTexture::ASTC_10x6;
  else if(requires { F::ASTC_10X8; })
    return QRhiTexture::ASTC_10x8;
  else if(requires { F::ASTC_10X10; })
    return QRhiTexture::ASTC_10x10;
  else if(requires { F::ASTC_12X10; })
    return QRhiTexture::ASTC_12x10;
  else if(requires { F::ASTC_12X12; })
    return QRhiTexture::ASTC_12x12;
  else
    return QRhiTexture::RGBA8;
}

struct DefaultPipeline
{
  struct layout
  {
    enum
    {
      graphics
    };
    struct vertex_input
    {
      struct
      {
        static constexpr auto name() { return "position"; }
        static constexpr int location() { return 0; }
        float data[2];
      } pos;
    };
    struct vertex_output
    {
    };
    struct fragment_input
    {
    };
    struct bindings
    {
    };
  };

  static QString vertex()
  {
    return R"_(#version 450
layout(location = 0) in vec2 position;
out gl_PerVertex { vec4 gl_Position; };
void main() {
  gl_Position = vec4( position, 0.0, 1.0 );
}
)_";
  }
};

template <typename C>
constexpr auto usage()
{
  if constexpr(requires { C::vertex; })
    return QRhiBuffer::VertexBuffer;
  else if constexpr(requires { C::index; })
    return QRhiBuffer::IndexBuffer;
  else if constexpr(requires { C::ubo; })
    return QRhiBuffer::UniformBuffer;
  else if constexpr(requires { C::storage; })
    return QRhiBuffer::StorageBuffer;
  else
  {
    static_assert(C::unhandled);
    throw;
  }
}

template <typename C>
constexpr auto buffer_type()
{
  if constexpr(requires { C::immutable; })
    return QRhiBuffer::Immutable;
  else if constexpr(requires { C::static_; })
    return QRhiBuffer::Static;
  else if constexpr(requires { C::dynamic; })
    return QRhiBuffer::Dynamic;
  else
  {
    static_assert(C::unhandled);
    throw;
  }
}

template <typename C>
auto samples(C c)
{
  if constexpr(requires { C::samples; })
    return c.samples;
  else
    return -1;
}

#include <Gfx/Qt5CompatPush> // clang-format: keep

struct handle_release
{
  QRhi& rhi;
  template <typename C>
  void operator()(C command)
  {
    if constexpr(requires { C::deallocation; })
    {
      if constexpr(
          requires { C::vertex; } || requires { C::index; } || requires { C::ubo; })
      {
        auto buf = reinterpret_cast<QRhiBuffer*>(command.handle);
        buf->deleteLater();
      }
      else if constexpr(requires { C::sampler; })
      {
        auto buf = reinterpret_cast<QRhiSampler*>(command.handle);
        buf->deleteLater();
      }
      else if constexpr(requires { C::texture; })
      {
        auto buf = reinterpret_cast<QRhiTexture*>(command.handle);
        buf->deleteLater();
      }
      else
      {
        static_assert(C::unhandled);
      }
    }
    else
    {
      static_assert(C::unhandled);
    }
  }
};

template <typename Self, typename Res>
struct handle_dispatch
{
  Self& self;
  QRhi& rhi;
  QRhiCommandBuffer& cb;
  QRhiResourceUpdateBatch*& res;
  QRhiComputePipeline& pip;
  template <typename C>
  Res operator()(C command)
  {
    if constexpr(requires { C::compute; })
    {
      if constexpr(requires { C::dispatch; })
      {
        cb.dispatch(command.x, command.y, command.z);
        return {};
      }
      else if constexpr(requires { C::begin; })
      {
        cb.beginComputePass(res);
        res = nullptr;
        cb.setComputePipeline(&pip);
        cb.setShaderResources(pip.shaderResourceBindings());

        return {};
      }
      else if constexpr(requires { C::end; })
      {
        cb.endComputePass(res);
        res = nullptr;
        rhi.finish();
        return {};
      }
      else
      {
        static_assert(C::unhandled);
        return {};
      }
    }
    else if constexpr(requires { C::readback; })
    {
      // First handle the readback request
      if constexpr(requires { C::request; })
      {
        if constexpr(requires { C::buffer; })
        {
          using ret = typename C::return_type;

          auto readback = new QRhiBufferReadbackResult;
          self.addReadback(readback);

          // this is e.g. a buffer_awaiter
          ret user_rb{.handle = reinterpret_cast<decltype(ret::handle)>(readback)};

          // TODO: do it with coroutines like this for peak asyncess
          // ret must be a coroutine type.
          // When the GPU completes the work, "completed" is called:
          // this will cause the coroutine to be filled with the data
          // readback->completed = [=] {
          //   qDebug() << "alhamdullilah he will be baked";
          //   // store "data" in the coroutine
          // };

          auto next = rhi.nextResourceUpdateBatch();
          auto buf = reinterpret_cast<QRhiBuffer*>(command.handle);

          next->readBackBuffer(buf, command.offset, command.size, readback);
          res = next;

          return user_rb;
        }
        else if constexpr(requires { C::texture; })
        {
          using ret = typename C::return_type;
          QRhiReadbackResult readback;
          return ret{};
        }
        else
        {
          static_assert(C::unhandled);
          return {};
        }
      }
      else if constexpr(requires { C::await; })
      {
        if constexpr(requires { C::buffer; })
        {
          using ret = typename C::return_type;

          auto readback = reinterpret_cast<QRhiBufferReadbackResult*>(command.handle);

          return ret{
              .data = readback->data.data(), .size = (std::size_t)readback->data.size()};
        }
        else if constexpr(requires { C::texture; })
        {
          using ret = typename C::return_type;

          auto readback = reinterpret_cast<QRhiReadbackResult*>(command.handle);

          return ret{
              .data = readback->data.data(), .size = (std::size_t)readback->data.size()};
        }
      }
    }
    else
    {
      static_assert(C::unhandled);
      return {};
    }

    return {};
  }
};

template <typename Self, typename Ret>
struct handle_update
{
  Self& self;
  QRhi& rhi;
  QRhiResourceUpdateBatch& res;
  std::vector<QRhiShaderResourceBinding>& srb;
  bool& srb_touched;

  template <typename C>
  Ret operator()(C command)
  {
    if constexpr(requires { C::allocation; })
    {
      if constexpr(
          requires { C::vertex; } || requires { C::index; })
      {
        auto buf = rhi.newBuffer(buffer_type<C>(), usage<C>(), command.size);
        buf->create();
        return reinterpret_cast<typename C::return_type>(buf);
      }
      else if constexpr(requires { C::sampler; })
      {
        auto buf = rhi.newSampler({}, {}, {}, {}, {});
        buf->create();
        return reinterpret_cast<typename C::return_type>(buf);
      }
      else if constexpr(
          requires { C::ubo; } || requires { C::storage; })
      {
        auto buf = rhi.newBuffer(buffer_type<C>(), usage<C>(), command.size);
        buf->create();

        // Replace it in our bindings
        score::gfx::replaceBuffer(srb, command.binding, buf);
        srb_touched = true;
        return reinterpret_cast<typename C::return_type>(buf);
      }
      else if constexpr(requires { C::texture; })
      {
        auto tex = rhi.newTexture(
            QRhiTexture::RGBA8, QSize{command.width, command.height}, samples(command));
        tex->create();

        score::gfx::replaceTexture(srb, command.binding, tex);
        srb_touched = true;
        return reinterpret_cast<typename C::return_type>(tex);
      }
      else
      {
        static_assert(C::unhandled);
        return {};
      }
    }
    else if constexpr(requires { C::upload; })
    {
      if constexpr(requires { C::texture; })
      {
        QRhiTextureSubresourceUploadDescription sub{command.data, command.size};
        res.uploadTexture(
            reinterpret_cast<QRhiTexture*>(command.handle),
            QRhiTextureUploadDescription{{0, 0, sub}});
      }
      else
      {
        auto buf = reinterpret_cast<QRhiBuffer*>(command.handle);
        if constexpr(requires { C::dynamic; })
          res.updateDynamicBuffer(buf, command.offset, command.size, command.data);
        else if constexpr(
            requires { C::static_; } || requires { C::immutable; })
          res.uploadStaticBuffer(buf, command.offset, command.size, command.data);
        else
        {
          static_assert(C::unhandled);
          return {};
        }
      }
    }
    else if constexpr(requires { C::getter; })
    {
      if constexpr(requires { C::ubo; })
      {
        auto buf = self.createdUbos.at(command.binding);
        return reinterpret_cast<typename C::return_type>(buf);
      }
      else
      {
        static_assert(C::unhandled);
        return {};
      }
    }
    else
    {
      handle_release{rhi}(command);
      return {};
    }
    return {};
  }
};

#include <Gfx/Qt5CompatPop> // clang-format: keep

struct generate_shaders
{
  template <typename T, int N>
  using vec = T[N];

  static constexpr std::string_view field_type(float) { return "float"; }
  static constexpr std::string_view field_type(const float (&)[2]) { return "vec2"; }
  static constexpr std::string_view field_type(const float (&)[3]) { return "vec3"; }
  static constexpr std::string_view field_type(const float (&)[4]) { return "vec4"; }

  static constexpr std::string_view field_type(float*) { return "float"; }
  static constexpr std::string_view field_type(vec<float, 2>*) { return "vec2"; }
  static constexpr std::string_view field_type(vec<float, 3>*) { return "vec3"; }
  static constexpr std::string_view field_type(vec<float, 4>*) { return "vec4"; }

  static constexpr std::string_view field_type(int) { return "int"; }
  static constexpr std::string_view field_type(const int (&)[2]) { return "ivec2"; }
  static constexpr std::string_view field_type(const int (&)[3]) { return "ivec3"; }
  static constexpr std::string_view field_type(const int (&)[4]) { return "ivec4"; }

  static constexpr std::string_view field_type(int*) { return "int"; }
  static constexpr std::string_view field_type(vec<int, 2>*) { return "ivec2"; }
  static constexpr std::string_view field_type(vec<int, 3>*) { return "ivec3"; }
  static constexpr std::string_view field_type(vec<int, 4>*) { return "ivec4"; }

  static constexpr std::string_view field_type(uint32_t) { return "uint"; }
  static constexpr std::string_view field_type(const uint32_t (&)[2]) { return "uvec2"; }
  static constexpr std::string_view field_type(const uint32_t (&)[3]) { return "uvec3"; }
  static constexpr std::string_view field_type(const uint32_t (&)[4]) { return "uvec4"; }

  static constexpr std::string_view field_type(uint32_t*) { return "uint"; }
  static constexpr std::string_view field_type(vec<uint32_t, 2>*) { return "uvec2"; }
  static constexpr std::string_view field_type(vec<uint32_t, 3>*) { return "uvec3"; }
  static constexpr std::string_view field_type(vec<uint32_t, 4>*) { return "uvec4"; }

  static constexpr std::string_view field_type(avnd::xy_value auto) { return "vec2"; }

  static constexpr bool field_array(float) { return false; }
  static constexpr bool field_array(const float (&)[2]) { return false; }
  static constexpr bool field_array(const float (&)[3]) { return false; }
  static constexpr bool field_array(const float (&)[4]) { return false; }

  static constexpr bool field_array(float*) { return true; }
  static constexpr bool field_array(vec<float, 2>*) { return true; }
  static constexpr bool field_array(vec<float, 3>*) { return true; }
  static constexpr bool field_array(vec<float, 4>*) { return true; }

  static constexpr bool field_array(int) { return false; }
  static constexpr bool field_array(const int (&)[2]) { return false; }
  static constexpr bool field_array(const int (&)[3]) { return false; }
  static constexpr bool field_array(const int (&)[4]) { return false; }

  static constexpr bool field_array(int*) { return true; }
  static constexpr bool field_array(vec<int, 2>*) { return true; }
  static constexpr bool field_array(vec<int, 3>*) { return true; }
  static constexpr bool field_array(vec<int, 4>*) { return true; }

  static constexpr bool field_array(uint32_t) { return false; }
  static constexpr bool field_array(const uint32_t (&)[2]) { return false; }
  static constexpr bool field_array(const uint32_t (&)[3]) { return false; }
  static constexpr bool field_array(const uint32_t (&)[4]) { return false; }

  static constexpr bool field_array(uint32_t*) { return true; }
  static constexpr bool field_array(vec<uint32_t, 2>*) { return true; }
  static constexpr bool field_array(vec<uint32_t, 3>*) { return true; }
  static constexpr bool field_array(vec<uint32_t, 4>*) { return true; }

  static constexpr bool field_array(avnd::xy_value auto) { return false; }

  template <typename T>
  static constexpr std::string_view image_qualifier()
  {
    if constexpr(requires { T::readonly; })
      return "readonly";
    else if constexpr(requires { T::writeonly; })
      return "writeonly";
    else
      static_assert(T::readonly || T::writeonly);
  }

  struct write_input
  {
    std::string& shader;

    template <typename T>
    void operator()(const T& field)
    {
      shader += fmt::format(
          "layout(location = {}) in {} {};\n", T::location(), field_type(field.data),
          T::name());
    }
  };

  struct write_output
  {
    std::string& shader;

    template <typename T>
    void operator()(const T& field)
    {
      if constexpr(requires { field.location(); })
      {
        shader += fmt::format(
            "layout(location = {}) out {} {};\n", T::location(), field_type(field.data),
            T::name());
      }
    }
  };

  struct write_binding
  {
    std::string& shader;

    template <typename T>
    void operator()(const T& field)
    {
      shader += fmt::format(
          "  {} {}{};\n", field_type(field.value), T::name(),
          field_array(field.value) ? "[]" : "");
    }
  };

  struct write_bindings
  {
    std::string& shader;

    template <typename C>
    void operator()(const C& field)
    {
      if constexpr(requires { C::sampler2D; })
      {
        shader += fmt::format(
            "layout(binding = {}) uniform sampler2D {};\n\n", C::binding(), C::name());
      }
      else if constexpr(requires { C::image2D; })
      {
        shader += fmt::format(
            "layout(binding = {}, {}) {} uniform image2D {};\n\n", C::binding(),
            C::format(), image_qualifier<C>(), C::name());
      }
      else if constexpr(requires { C::ubo; })
      {
        shader += fmt::format(
            "layout({}, binding = {}) uniform {}\n{{\n",
            "std140" // TODO
            ,
            C::binding(), C::name());

        boost::pfr::for_each_field(field, write_binding{shader});

        shader += fmt::format("}};\n\n");
      }
      else if constexpr(requires { C::buffer; })
      {
        shader += fmt::format(
            "layout({}, binding = {}) buffer {}\n{{\n",
            "std140" // TODO
            ,
            C::binding(), C::name());

        boost::pfr::for_each_field(field, write_binding{shader});

        shader += fmt::format("}};\n\n");
      }
    }
  };

  template <typename T>
  std::string vertex_shader(const T& lay)
  {
    using namespace gpp::qrhi;
    std::string shader = "#version 450\n\n";

    if constexpr(requires { lay.vertex_input; })
      boost::pfr::for_each_field(lay.vertex_input, write_input{shader});
    else if constexpr(requires { typename T::vertex_input; })
      boost::pfr::for_each_field(typename T::vertex_input{}, write_input{shader});
    else
      boost::pfr::for_each_field(
          DefaultPipeline::layout::vertex_input{}, write_input{shader});

    if constexpr(requires { lay.vertex_output; })
      boost::pfr::for_each_field(lay.vertex_output, write_output{shader});
    else if constexpr(requires { typename T::vertex_output; })
      boost::pfr::for_each_field(typename T::vertex_output{}, write_output{shader});

    shader += "\n";

    if constexpr(requires { lay.bindings; })
      boost::pfr::for_each_field(lay.bindings, write_bindings{shader});
    else if constexpr(requires { typename T::bindings; })
      boost::pfr::for_each_field(typename T::bindings{}, write_bindings{shader});

    return shader;
  }

  template <typename T>
  std::string fragment_shader(const T& lay)
  {
    std::string shader = "#version 450\n\n";

    if constexpr(requires { lay.fragment_input; })
      boost::pfr::for_each_field(lay.fragment_input, write_input{shader});
    else if constexpr(requires { typename T::fragment_input; })
      boost::pfr::for_each_field(typename T::fragment_input{}, write_input{shader});

    if constexpr(requires { lay.fragment_output; })
      boost::pfr::for_each_field(lay.fragment_output, write_output{shader});
    else if constexpr(requires { typename T::fragment_output; })
      boost::pfr::for_each_field(typename T::fragment_output{}, write_output{shader});

    shader += "\n";

    if constexpr(requires { lay.bindings; })
      boost::pfr::for_each_field(lay.bindings, write_bindings{shader});
    else if constexpr(requires { typename T::bindings; })
      boost::pfr::for_each_field(typename T::bindings{}, write_bindings{shader});

    return shader;
  }

  template <typename T>
  std::string compute_shader(const T& lay)
  {
    std::string fstr = "#version 450\n\n";

    fstr += "layout(";
    if constexpr(requires { T::local_size_x(); })
    {
      fstr += fmt::format("local_size_x = {}, ", T::local_size_x());
    }
    if constexpr(requires { T::local_size_y(); })
    {
      fstr += fmt::format("local_size_y = {}, ", T::local_size_y());
    }
    if constexpr(requires { T::local_size_z(); })
    {
      fstr += fmt::format("local_size_z = {}, ", T::local_size_z());
    }

    // Remove the last ", "
    fstr.resize(fstr.size() - 2);
    fstr += ") in;\n\n";

    boost::pfr::for_each_field(lay.bindings, write_bindings{fstr});

    return fstr;
  }
};
}

namespace oscr
{
struct GpuControlOuts
{
  std::weak_ptr<Execution::ExecutionCommandQueue> queue;
  Gfx::exec_controls control_outs;

  int instance{};

  template <typename Node_T>
  void processControlOut(Node_T& state) const noexcept
  {
    if(!this->control_outs.empty())
    {
      auto q = this->queue.lock();
      if(!q)
        return;
      auto& qq = *q;
      int parm_k = 0;
      avnd::parameter_output_introspection<Node_T>::for_all(
          avnd::get_outputs(state), [&]<avnd::parameter T>(const T& t) {
            qq.enqueue([v = oscr::to_ossia_value(t, t.value),
                        port = control_outs[parm_k]]() mutable {
              std::swap(port->value, v);
              port->changed = true;
            });

            parm_k++;
          });
    }
  }
};

struct CustomGpuNodeBase
    : score::gfx::Node
    , GpuControlOuts
{
  CustomGpuNodeBase(
      std::weak_ptr<Execution::ExecutionCommandQueue>&& q, Gfx::exec_controls&& ctls)
      : GpuControlOuts{std::move(q), std::move(ctls)}
  {
  }

  virtual ~CustomGpuNodeBase() = default;

  QString vertex, fragment, compute;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
};

struct CustomGpuOutputNodeBase
    : score::gfx::OutputNode
    , GpuControlOuts
{
  CustomGpuOutputNodeBase(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls&& ctls);
  virtual ~CustomGpuOutputNodeBase() = default;

  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::function<void()> m_update;

  QString vertex, fragment, compute;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;

  void setRenderer(std::shared_ptr<score::gfx::RenderList>) override;
  score::gfx::RenderList* renderer() const override;

  void startRendering() override;
  void render() override;
  void stopRendering() override;
  bool canRender() const override;
  void onRendererChange() override;

  void createOutput(
      score::gfx::GraphicsApi graphicsApi, std::function<void()> onReady,
      std::function<void()> onUpdate, std::function<void()> onResize) override;

  void destroyOutput() override;
  std::shared_ptr<score::gfx::RenderState> renderState() const override;

  Configuration configuration() const noexcept override;
};

template <typename Node_T, typename Node>
void prepareNewState(Node_T& node, const Node& parent)
{
  if constexpr(avnd::can_prepare<Node_T>)
  {
    using prepare_type = avnd::first_argument<&Node_T::prepare>;
    prepare_type t;
    if_possible(t.instance = parent.instance);
    node.prepare(t);
  }
}

}

#endif
