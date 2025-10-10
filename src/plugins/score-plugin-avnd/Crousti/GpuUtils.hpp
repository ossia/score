#pragma once

#if SCORE_PLUGIN_GFX
#include <Process/ExecutionContext.hpp>

#include <Crousti/File.hpp>
#include <Crousti/MessageBus.hpp>
#include <Gfx/GfxExecNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderState.hpp>

#include <score/tools/ThreadPool.hpp>

#include <ossia-qt/invoke.hpp>

#include <QCoreApplication>
#include <QTimer>
#include <QtGui/private/qrhi_p.h>

#include <avnd/binding/ossia/port_run_postprocess.hpp>
#include <avnd/binding/ossia/port_run_preprocess.hpp>
#include <avnd/binding/ossia/soundfiles.hpp>
#include <avnd/concepts/parameter.hpp>
#include <avnd/introspection/input.hpp>
#include <avnd/introspection/output.hpp>
#include <fmt/format.h>
#include <gpp/layout.hpp>

#include <score_plugin_avnd_export.h>

namespace gpp::qrhi
{

template <typename F>
  requires std::is_enum_v<F>
constexpr QRhiTexture::Format textureFormat(F f) noexcept
{
  if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
    if(f == F::RGBA8)
      return QRhiTexture::RGBA8;
  if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
    if(f == F::BGRA8)
      return QRhiTexture::BGRA8;
  if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
    if(f == F::R8)
      return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::RG8; })
    if(f == F::RG8)
      return QRhiTexture::RG8;
#endif
  if constexpr(requires { F::R16; })
    if(f == F::R16)
      return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::RG16; })
    if(f == F::RG16)
      return QRhiTexture::RG16;
#endif
  if constexpr(requires { F::RED_OR_ALPHA8; })
    if(f == F::RED_OR_ALPHA8)
      return QRhiTexture::RED_OR_ALPHA8;
  if constexpr(requires { F::RGBA16F; })
    if(f == F::RGBA16F)
      return QRhiTexture::RGBA16F;
  if constexpr(requires { F::RGBA32F; })
    if(f == F::RGBA32F)
      return QRhiTexture::RGBA32F;
  if constexpr(requires { F::R16F; })
    if(f == F::R16F)
      return QRhiTexture::R16F;
  if constexpr(requires { F::R32F; })
    if(f == F::R32F)
      return QRhiTexture::R32F;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::RGB10A2; })
    if(f == F::RGB10A2)
      return QRhiTexture::RGB10A2;
#endif
  if constexpr(requires { F::D16; })
    if(f == F::D16)
      return QRhiTexture::D16;

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
  if constexpr(requires { F::D24; })
    if(f == F::D24)
      return QRhiTexture::D24;
  if constexpr(requires { F::D24S8; })
    if(f == F::D24S8)
      return QRhiTexture::D24S8;
#endif
  if constexpr(requires { F::D32F; })
    if(f == F::D32F)
      return QRhiTexture::D32F;

  if constexpr(requires { F::BC1; })
    if(f == F::BC1)
      return QRhiTexture::BC1;
  if constexpr(requires { F::BC2; })
    if(f == F::BC2)
      return QRhiTexture::BC2;
  if constexpr(requires { F::BC3; })
    if(f == F::BC3)
      return QRhiTexture::BC3;
  if constexpr(requires { F::BC4; })
    if(f == F::BC4)
      return QRhiTexture::BC4;
  if constexpr(requires { F::BC5; })
    if(f == F::BC5)
      return QRhiTexture::BC5;
  if constexpr(requires { F::BC6H; })
    if(f == F::BC6H)
      return QRhiTexture::BC6H;
  if constexpr(requires { F::BC7; })
    if(f == F::BC7)
      return QRhiTexture::BC7;
  if constexpr(requires { F::ETC2_RGB8; })
    if(f == F::ETC2_RGB8)
      return QRhiTexture::ETC2_RGB8;
  if constexpr(requires { F::ETC2_RGB8A1; })
    if(f == F::ETC2_RGB8A1)
      return QRhiTexture::ETC2_RGB8A1;
  if constexpr(requires { F::ETC2_RGB8A8; })
    if(f == F::ETC2_RGBA8)
      return QRhiTexture::ETC2_RGBA8;
  if constexpr(requires { F::ASTC_4X4; })
    if(f == F::ASTC_4x4)
      return QRhiTexture::ASTC_4x4;
  if constexpr(requires { F::ASTC_5X4; })
    if(f == F::ASTC_5x4)
      return QRhiTexture::ASTC_5x4;
  if constexpr(requires { F::ASTC_5X5; })
    if(f == F::ASTC_5x5)
      return QRhiTexture::ASTC_5x5;
  if constexpr(requires { F::ASTC_6X5; })
    if(f == F::ASTC_6x5)
      return QRhiTexture::ASTC_6x5;
  if constexpr(requires { F::ASTC_6X6; })
    if(f == F::ASTC_6x6)
      return QRhiTexture::ASTC_6x6;
  if constexpr(requires { F::ASTC_8X5; })
    if(f == F::ASTC_8x5)
      return QRhiTexture::ASTC_8x5;
  if constexpr(requires { F::ASTC_8X6; })
    if(f == F::ASTC_8x6)
      return QRhiTexture::ASTC_8x6;
  if constexpr(requires { F::ASTC_8X8; })
    if(f == F::ASTC_8x8)
      return QRhiTexture::ASTC_8x8;
  if constexpr(requires { F::ASTC_10X5; })
    if(f == F::ASTC_10x5)
      return QRhiTexture::ASTC_10x5;
  if constexpr(requires { F::ASTC_10X6; })
    if(f == F::ASTC_10x6)
      return QRhiTexture::ASTC_10x6;
  if constexpr(requires { F::ASTC_10X8; })
    if(f == F::ASTC_10x8)
      return QRhiTexture::ASTC_10x8;
  if constexpr(requires { F::ASTC_10X10; })
    if(f == F::ASTC_10x10)
      return QRhiTexture::ASTC_10x10;
  if constexpr(requires { F::ASTC_12X10; })
    if(f == F::ASTC_12x10)
      return QRhiTexture::ASTC_12x10;
  if constexpr(requires { F::ASTC_12X12; })
    if(f == F::ASTC_12x12)
      return QRhiTexture::ASTC_12x12;
  if constexpr(requires { F::RGB; })
    if(f == F::RGB)
      return QRhiTexture::RGBA8; // we'll have a CPU step to go to rgb

  return QRhiTexture::RGBA8;
}

template <typename F>
constexpr QRhiTexture::Format textureFormat() noexcept
{
  if constexpr(requires { std::string_view{F::format()}; })
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
  else if(fmt == "rgb")
    return QRhiTexture::RGBA8;
  else
    return QRhiTexture::RGBA8;
  }
  else if constexpr(std::is_enum_v<typename F::format>)
  {
    if constexpr(requires { F::RGBA; } || requires { F::RGBA8; })
      return QRhiTexture::RGBA8;
    else if constexpr(requires { F::BGRA; } || requires { F::BGRA8; })
      return QRhiTexture::BGRA8;
    else if constexpr(requires { F::R8; } || requires { F::GRAYSCALE; })
      return QRhiTexture::R8;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if constexpr(requires { F::RG8; })
      return QRhiTexture::RG8;
#endif
    else if constexpr(requires { F::R16; })
      return QRhiTexture::R16;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    else if constexpr(requires { F::RG16; })
      return QRhiTexture::RG16;
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
    else if constexpr(requires { F::ETC2_RGB8; })
      return QRhiTexture::ETC2_RGB8;
    else if constexpr(requires { F::ETC2_RGB8A1; })
      return QRhiTexture::ETC2_RGB8A1;
    else if constexpr(requires { F::ETC2_RGB8A8; })
      return QRhiTexture::ETC2_RGBA8;
    else if constexpr(requires { F::ASTC_4X4; })
      return QRhiTexture::ASTC_4x4;
    else if constexpr(requires { F::ASTC_5X4; })
      return QRhiTexture::ASTC_5x4;
    else if constexpr(requires { F::ASTC_5X5; })
      return QRhiTexture::ASTC_5x5;
    else if constexpr(requires { F::ASTC_6X5; })
      return QRhiTexture::ASTC_6x5;
    else if constexpr(requires { F::ASTC_6X6; })
      return QRhiTexture::ASTC_6x6;
    else if constexpr(requires { F::ASTC_8X5; })
      return QRhiTexture::ASTC_8x5;
    else if constexpr(requires { F::ASTC_8X6; })
      return QRhiTexture::ASTC_8x6;
    else if constexpr(requires { F::ASTC_8X8; })
      return QRhiTexture::ASTC_8x8;
    else if constexpr(requires { F::ASTC_10X5; })
      return QRhiTexture::ASTC_10x5;
    else if constexpr(requires { F::ASTC_10X6; })
      return QRhiTexture::ASTC_10x6;
    else if constexpr(requires { F::ASTC_10X8; })
      return QRhiTexture::ASTC_10x8;
    else if constexpr(requires { F::ASTC_10X10; })
      return QRhiTexture::ASTC_10x10;
    else if constexpr(requires { F::ASTC_12X10; })
      return QRhiTexture::ASTC_12x10;
    else if constexpr(requires { F::ASTC_12X12; })
      return QRhiTexture::ASTC_12x12;
    else if constexpr(requires { F::RGB; })
      return QRhiTexture::RGBA8;
    else
      return QRhiTexture::RGBA8;
  }
}

template <avnd::cpu_texture Tex>
constexpr QRhiTexture::Format textureFormat(const Tex& t) noexcept
{
  QRhiTexture::Format fmt{};
  if constexpr(avnd::cpu_dynamic_format_texture<Tex>)
    fmt = gpp::qrhi::textureFormat(t.format);
  else
    fmt = gpp::qrhi::textureFormat<Tex>();
  return fmt;
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

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
          auto readback = new QRhiBufferReadbackResult;
#else
          auto readback = new QRhiReadbackResult;
#endif
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

#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
          auto readback = reinterpret_cast<QRhiBufferReadbackResult*>(command.handle);
#else
          auto readback = reinterpret_cast<QRhiReadbackResult*>(command.handle);
#endif

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
        QRhiTextureSubresourceUploadDescription sub(command.data, command.size);
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
struct GpuWorker
{
  template <typename T>
  void initWorker(this auto& self, std::shared_ptr<T>& state) noexcept
  {
    if constexpr(avnd::has_worker<T>)
    {
      auto ptr = QPointer{&self};
      auto& tq = score::TaskPool::instance();
      using worker_type = decltype(state->worker);

      auto wk_state = std::weak_ptr{state};
      state->worker.request = [ptr, &tq, wk_state]<typename... Args>(Args&&... f) {
        using type_of_result = decltype(worker_type::work(std::forward<Args>(f)...));
        tq.post([... ff = std::forward<Args>(f), wk_state, ptr]() mutable {
          if constexpr(std::is_void_v<type_of_result>)
          {
            worker_type::work(std::forward<decltype(ff)>(ff)...);
          }
          else
          {
            // If the worker returns a std::function, it
            // is to be invoked back in the processor DSP thread
            auto res = worker_type::work(std::forward<decltype(ff)>(ff)...);
            if(!res || !ptr)
              return;

            ossia::qt::run_async(
                QCoreApplication::instance(),
                [res = std::move(res), wk_state, ptr]() mutable {
              if(ptr)
                if(auto state = wk_state.lock())
                  res(*state);
                });
          }
        });
      };
    }
  }
};

template <typename GpuNodeRenderer, typename Node>
struct GpuProcessIns
{
  GpuNodeRenderer& gpu;
  Node& state;
  const score::gfx::Message& prev_mess;
  const score::gfx::Message& mess;
  const score::DocumentContext& ctx;

  bool can_process_message(std::size_t N)
  {
    if(mess.input.size() <= N)
      return false;

    if(prev_mess.input.size() == mess.input.size())
    {
      auto& prev = prev_mess.input[N];
      auto& next = mess.input[N];
      if(prev.index() == 1 && next.index() == 1)
      {
        if(ossia::get<ossia::value>(prev) == ossia::get<ossia::value>(next))
        {
          return false;
        }
      }
    }
    return true;
  }

  void operator()(avnd::parameter auto& t, auto field_index)
  {
    if(!can_process_message(field_index))
      return;

    if(auto val = ossia::get_if<ossia::value>(&mess.input[field_index]))
    {
      oscr::from_ossia_value(t, *val, t.value);
      if_possible(t.update(state));
    }
  }

#if OSCR_HAS_MMAP_FILE_STORAGE
  template <avnd::raw_file_port Field, std::size_t NField>
  void operator()(Field& t, avnd::field_index<NField> field_index)
  {
    // FIXME we should be loading a file there
    using node_type = std::remove_cvref_t<decltype(gpu.node())>;
    using file_ports = avnd::raw_file_input_introspection<Node>;

    if(!can_process_message(field_index))
      return;

    auto val = ossia::get_if<ossia::value>(&mess.input[field_index]);
    if(!val)
      return;

    static constexpr bool has_text = requires { decltype(Field::file)::text; };
    static constexpr bool has_mmap = requires { decltype(Field::file)::mmap; };

    // First we can load it directly since execution hasn't started yet
    if(auto hdl = loadRawfile(*val, ctx, has_text, has_mmap))
    {
      static constexpr auto N = file_ports::field_index_to_index(NField);
      if constexpr(avnd::port_can_process<Field>)
      {
        // FIXME also do it when we get a run-time message from the exec engine,
        // OSC, etc
        auto func = executePortPreprocess<Field>(*hdl);
        const_cast<node_type&>(gpu.node())
            .file_loaded(
                state, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
        if(func)
          func(state);
      }
      else
      {
        const_cast<node_type&>(gpu.node())
            .file_loaded(
                state, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
      }
    }
  }
#endif

  template <avnd::texture_port Field, std::size_t NField>
  void operator()(Field& t, avnd::field_index<NField> field_index)
  {
    using node_type = std::remove_cvref_t<decltype(gpu.node())>;
    auto& node = const_cast<node_type&>(gpu.node());
    auto val = ossia::get_if<ossia::render_target_spec>(&mess.input[field_index]);
    if(!val)
      return;
    node.process(NField, *val);
  }

  void operator()(auto& t, auto field_index) = delete;
};

struct GpuControlIns
{
  template <typename Self, typename Node_T>
  static void processControlIn(
      Self& self, Node_T& state, score::gfx::Message& renderer_mess,
      const score::gfx::Message& mess, const score::DocumentContext& ctx) noexcept
  {
    // Apply the controls
    avnd::input_introspection<Node_T>::for_all_n(
        avnd::get_inputs<Node_T>(state),
        GpuProcessIns<Self, Node_T>{self, state, renderer_mess, mess, ctx});
    renderer_mess = mess;
  }
};

struct GpuControlOuts
{
  std::weak_ptr<Execution::ExecutionCommandQueue> queue;
  Gfx::exec_controls control_outs;

  int64_t instance{};

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

template <typename T>
struct SCORE_PLUGIN_AVND_EXPORT GpuNodeElements
{
  [[no_unique_address]] oscr::soundfile_storage<T> soundfiles;

  [[no_unique_address]] oscr::midifile_storage<T> midifiles;

#if defined(OSCR_HAS_MMAP_FILE_STORAGE)
  [[no_unique_address]] oscr::raw_file_storage<T> rawfiles;
#endif

  template <std::size_t N, std::size_t NField>
  void file_loaded(
      auto& state, const std::shared_ptr<oscr::raw_file_data>& hdl,
      avnd::predicate_index<N>, avnd::field_index<NField>)
  {
    this->rawfiles.load(
        state, hdl, avnd::predicate_index<N>{}, avnd::field_index<NField>{});
  }
};

struct SCORE_PLUGIN_AVND_EXPORT CustomGfxNodeBase : score::gfx::NodeModel
{
  explicit CustomGfxNodeBase(const score::DocumentContext& ctx)
      : score::gfx::NodeModel{}
      , m_ctx{ctx}
  {
  }
  virtual ~CustomGfxNodeBase();
  const score::DocumentContext& m_ctx;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
  using score::gfx::NodeModel::process;
};
struct SCORE_PLUGIN_AVND_EXPORT CustomGfxOutputNodeBase : score::gfx::OutputNode
{
  virtual ~CustomGfxOutputNodeBase();

  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
};
struct CustomGpuNodeBase
    : score::gfx::Node
    , GpuWorker
    , GpuControlIns
    , GpuControlOuts
{
  CustomGpuNodeBase(
      std::weak_ptr<Execution::ExecutionCommandQueue>&& q, Gfx::exec_controls&& ctls,
      const score::DocumentContext& ctx)
      : GpuControlOuts{std::move(q), std::move(ctls)}
      , m_ctx{ctx}
  {
  }

  virtual ~CustomGpuNodeBase() = default;

  const score::DocumentContext& m_ctx;
  QString vertex, fragment, compute;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
};

struct SCORE_PLUGIN_AVND_EXPORT CustomGpuOutputNodeBase
    : score::gfx::OutputNode
    , GpuWorker
    , GpuControlIns
    , GpuControlOuts
{
  CustomGpuOutputNodeBase(
      std::weak_ptr<Execution::ExecutionCommandQueue> q, Gfx::exec_controls&& ctls,
      const score::DocumentContext& ctx);
  virtual ~CustomGpuOutputNodeBase();

  const score::DocumentContext& m_ctx;
  std::weak_ptr<score::gfx::RenderList> m_renderer{};
  std::shared_ptr<score::gfx::RenderState> m_renderState{};
  std::function<void()> m_update;

  QString vertex, fragment, compute;
  score::gfx::Message last_message;
  void process(score::gfx::Message&& msg) override;
  using score::gfx::Node::process;

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
void prepareNewState(std::shared_ptr<Node_T>& eff, const Node& parent)
{
  if constexpr(avnd::has_worker<Node_T>)
  {
    parent.initWorker(eff);
  }
  if constexpr(avnd::has_processor_to_gui_bus<Node_T>)
  {
    auto& process = parent.processModel;
    eff->send_message = [ptr = QPointer{&process}](auto&& b) mutable {
      // FIXME right now all the rendering is done in the UI thread, which is very MEH
      //    this->in_edit([&process, bb = std::move(b)]() mutable {

      if(ptr && ptr->to_ui)
        MessageBusSender{ptr->to_ui}(std::move(b));
      //    });
    };

    // FIXME GUI -> engine. See executor.hpp
  }

  avnd::init_controls(*eff);

  if constexpr(avnd::can_prepare<Node_T>)
  {
    if constexpr(avnd::function_reflection<&Node_T::prepare>::count == 1)
    {
      using prepare_type = avnd::first_argument<&Node_T::prepare>;
      prepare_type t;
      if_possible(t.instance = parent.instance);
      eff->prepare(t);
    }
    else
    {
      eff->prepare();
    }
  }
}

struct port_to_type_enum
{
  template <std::size_t I, avnd::cpu_texture_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    using texture_type = std::remove_cvref_t<decltype(F::texture)>;
    return avnd::cpu_fixed_format_texture<texture_type> ? score::gfx::Types::Image
                                                        : score::gfx::Types::Buffer;
  }
  template <std::size_t I, avnd::sampler_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Image;
  }
  template <std::size_t I, avnd::image_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Image;
  }
  template <std::size_t I, avnd::attachment_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Image;
  }

  template <std::size_t I, avnd::geometry_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Geometry;
  }
  template <std::size_t I, avnd::mono_audio_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Audio;
  }
  template <std::size_t I, avnd::poly_audio_port F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Audio;
  }
  template <std::size_t I, avnd::int_parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Int;
  }
  template <std::size_t I, avnd::enum_parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Int;
  }
  template <std::size_t I, avnd::float_parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Float;
  }
  template <std::size_t I, avnd::parameter F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    using value_type = std::remove_cvref_t<decltype(F::value)>;

    if constexpr(std::is_aggregate_v<value_type>)
    {
      constexpr int sz = boost::pfr::tuple_size_v<value_type>;
      if constexpr(sz == 2)
      {
        return score::gfx::Types::Vec2;
      }
      else if constexpr(sz == 3)
      {
        return score::gfx::Types::Vec3;
      }
      else if constexpr(sz == 4)
      {
        return score::gfx::Types::Vec4;
      }
    }
    return score::gfx::Types::Empty;
  }
  template <std::size_t I, typename F>
  constexpr auto operator()(avnd::field_reflection<I, F> p)
  {
    return score::gfx::Types::Empty;
  }
};

template <typename Node_T>
inline void initGfxPorts(auto* self, auto& input, auto& output)
{
  avnd::input_introspection<Node_T>::for_all(
      [self, &input]<typename Field, std::size_t I>(avnd::field_reflection<I, Field> f) {
    static constexpr auto type = port_to_type_enum{}(f);
    input.push_back(new score::gfx::Port{self, {}, type, {}});
  });
  avnd::output_introspection<Node_T>::for_all(
      [self,
       &output]<typename Field, std::size_t I>(avnd::field_reflection<I, Field> f) {
    static constexpr auto type = port_to_type_enum{}(f);
    output.push_back(new score::gfx::Port{self, {}, type, {}});
  });
}

inline void
inplaceMirror(unsigned char* bytes, int width, int height, int bytes_per_pixel)
{
  if(width < 1 || height <= 1)
    return;
  const size_t row_size = width * bytes_per_pixel;

  auto temp_row = (unsigned char*)alloca(row_size);
  auto top = bytes;
  auto bottom = bytes + (height - 1) * row_size;

  while(top < bottom)
  {
    memcpy(temp_row, top, row_size);
    memcpy(top, bottom, row_size);
    memcpy(bottom, temp_row, row_size);

    top += row_size;
    bottom -= row_size;
  }
}

template <avnd::cpu_texture Tex>
void loadInputTexture(QRhi& rhi, auto& m_readbacks, Tex& cpu_tex, int k)
{
  auto& buf = m_readbacks[k].data;
  if(buf.size() < (qsizetype)cpu_tex.bytesize())
  {
    cpu_tex.bytes = nullptr;
  }
  else
  {
    if constexpr(requires { Tex::RGB; })
    {
      // RGBA -> RGB
      const QByteArray rgba = buf;
      QByteArray rgb;
      rgb.resize(cpu_tex.width * cpu_tex.height * 3);
      auto src = rgba.constData();
      auto dst = rgb.data();
      for(int rgb_byte = 0, rgba_byte = 0, N = rgb.size(); rgb_byte < N;)
      {
        dst[rgb_byte + 0] = src[rgba_byte + 0];
        dst[rgb_byte + 1] = src[rgba_byte + 1];
        dst[rgb_byte + 2] = src[rgba_byte + 2];
        rgb_byte += 3;
        rgba_byte += 4;
      }
      buf = rgb;
    }

    using components_type = std::decay_t<decltype(cpu_tex.bytes)>;
    cpu_tex.bytes = reinterpret_cast<components_type>(buf.data());

    if(rhi.isYUpInNDC())
      if(cpu_tex.width * cpu_tex.height > 0)
        inplaceMirror(
            reinterpret_cast<unsigned char*>(cpu_tex.bytes), cpu_tex.width,
            cpu_tex.height, cpu_tex.bytes_per_pixel);

    cpu_tex.changed = true;
  }
}
}

#endif
