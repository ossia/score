#pragma once
#include "Gfx/Graph/Utils.hpp"
#include <QString>
#include <QtGui/private/qrhi_p.h>
namespace gpp::qrhi
{

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
        // FIXME format
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

}
