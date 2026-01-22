#pragma once

#include <QtGui/private/qrhi_p.h>
#include <avnd/concepts/gfx.hpp>
#include <Crousti/TextureFormat.hpp>

#include <QFloat16>

namespace gpp::qrhi
{

inline void toRGB(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
    case QRhiTexture::RGBA8:
    {
      // RGBA to
      // RGB
      uint8_t* data = reinterpret_cast<uint8_t*>(buf.data());
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        data[write_i + 0] = data[read_i + 0];
        data[write_i + 1] = data[read_i + 1];
        data[write_i + 2] = data[read_i + 2];
        write_i += 3;
        read_i += 4;
      }

      buf.resize(N * 3);
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA to
      // RGB
      uint8_t* data = reinterpret_cast<uint8_t*>(buf.data());
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        data[write_i + 0] = data[read_i + 2];
        data[write_i + 1] = data[read_i + 1];
        data[write_i + 2] = data[read_i + 0];
        write_i += 3;
        read_i += 4;
      }

      buf.resize(N * 3);
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
      // R to
      // RRR
      uint8_t* data = reinterpret_cast<uint8_t*>(buf.data());
      int write_i = 0;
      int read_i = 0;

#if QT_VERSION > QT_VERSION_CHECK(6,8,0)
      buf.resizeForOverwrite(N * 3);
#else
      buf.resize(N * 3);
#endif
      for(int i = N - 1; i >= 0; i--)
      {
        auto gray = data[i];
        data[i * 3 + 0] = gray;
        data[i * 3 + 1] = gray;
        data[i * 3 + 2] = gray;
      }
      break;
    }

    case QRhiTexture::R16: // x
    {
      // R to
      // RRR
      QByteArray rgb(N*3, Qt::Uninitialized);

      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(rgb.data());

      for(int i = 0; i < N; i++)
      {
        uint8_t gray = (uint32_t(src[i]) * 255) / 65535;

        dst[i * 3 + 0] = gray;
        dst[i * 3 + 1] = gray;
        dst[i * 3 + 2] = gray;
      }

      buf = rgb;
      break;
    }
    case QRhiTexture::RGBA16F:
    {
      // RGBA to
      // RGB
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      unsigned char* data = (unsigned char*)buf.data();
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        data[write_i + 0] = qBound(0, int(src[read_i + 0] * 255.0f), 255);
        data[write_i + 1] = qBound(0, int(src[read_i + 1] * 255.0f), 255);
        data[write_i + 2] = qBound(0, int(src[read_i + 2] * 255.0f), 255);
        write_i += 3;
        read_i += 4;
      }
      buf.resize(N * 3);
      break;
    }
    case QRhiTexture::RGBA32F:
    {
      // RGBA to
      // RGB
      auto src = reinterpret_cast<const float*>(buf.constData());
      unsigned char* data = (unsigned char*)buf.data();
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        data[write_i + 0] = qBound(0, int(src[read_i + 0] * 255.0f), 255);
        data[write_i + 1] = qBound(0, int(src[read_i + 1] * 255.0f), 255);
        data[write_i + 2] = qBound(0, int(src[read_i + 2] * 255.0f), 255);
        write_i += 3;
        read_i += 4;
      }
      buf.resize(N * 3);
      break;
    }
    case QRhiTexture::R16F:
    {
      // R to
      // RRR
      buf.resize(N * 3);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      unsigned char* dst = (unsigned char*)buf.data();
      for(int i = N - 1; i >= 0; i--)
      {
        uint8_t gray = qBound(0, int(src[i] * 255.0f), 255);
        dst[i * 3 + 0] = gray;
        dst[i * 3 + 1] = gray;
        dst[i * 3 + 2] = gray;
      }
      break;
    }
    case QRhiTexture::R32F:
    {
      // R32F to RGB8
      buf.resize(N * 3);
      auto src = reinterpret_cast<const float*>(buf.constData());
      unsigned char* dst = (unsigned char*)buf.data();
      int write_i = 0;
      for(int i = 0; i < N; i++)
      {
        uint8_t gray = qBound(0, int(src[i] * 255.0f), 255);
        dst[write_i + 0] = gray;
        dst[write_i + 1] = gray;
        dst[write_i + 2] = gray;
        write_i += 3;
      }
      buf.resize(N * 3);
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to RGB8 (already uint8, just expand)
      buf.resize(N * 3);
      unsigned char* data = (unsigned char*)buf.data();
      for(int i = N - 1; i >= 0; i--)
      {
        auto gray = data[i];
        data[i * 3 + 0] = gray;
        data[i * 3 + 1] = gray;
        data[i * 3 + 2] = gray;
      }
      break;
    }
    case QRhiTexture::R32UI:
    {
      // R32UI to
      // RGB8
      QByteArray rgb(N * 3, Qt::Uninitialized);
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(rgb.data());
      for(int i = 0; i < N; i++)
      {
        uint8_t gray = (uint64_t(src[i]) * 255) / 4294967295UL;
        dst[i * 3 + 0] = gray;
        dst[i * 3 + 1] = gray;
        dst[i * 3 + 2] = gray;
      }
      buf = rgb;
      break;
    }
    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to
      // RGB8
      QByteArray rgb(N * 3, Qt::Uninitialized);
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(rgb.data());
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        // Clamp uint32 values to uint8 range
        dst[write_i + 0] = src[read_i + 0] > 255 ? 255 : src[read_i + 0];
        dst[write_i + 1] = src[read_i + 1] > 255 ? 255 : src[read_i + 1];
        dst[write_i + 2] = src[read_i + 2] > 255 ? 255 : src[read_i + 2];
        write_i += 3;
        read_i += 4;
      }
      buf = rgb;
      break;
    }
#endif

    default:
      if(buf.size() < N * 3)
        buf.resize(N * 3);
      break;
  }
}

inline void toRGBA(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
    case QRhiTexture::RGBA8:
      break;

    case QRhiTexture::BGRA8:
    {
      // BGRA to RGBA (swap R and B in-place)
      uint8_t* data = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        std::swap(data[i * 4 + 0], data[i * 4 + 2]);
      }
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
#if QT_VERSION > QT_VERSION_CHECK(6,8,0)
      buf.resizeForOverwrite(N * 4);
#else
      buf.resize(N * 4);
#endif
      uint8_t* data = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        auto gray = data[i];
        data[i * 4 + 0] = gray;
        data[i * 4 + 1] = gray;
        data[i * 4 + 2] = gray;
        data[i * 4 + 3] = 255;
      }
      break;
    }

    case QRhiTexture::R16:
    {
      // R16 to RGBA8
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint8_t gray = (uint32_t(src[i]) * 255) / 65535;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 255;
      }
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to RGBA8
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        dst[write_i + 0] = qBound(0, int(src[read_i + 0] * 255.0f), 255);
        dst[write_i + 1] = qBound(0, int(src[read_i + 1] * 255.0f), 255);
        dst[write_i + 2] = qBound(0, int(src[read_i + 2] * 255.0f), 255);
        dst[write_i + 3] = qBound(0, int(src[read_i + 3] * 255.0f), 255);
        write_i += 4;
        read_i += 4;
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::RGBA32F:
    {
      // RGBA32F to RGBA8
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        dst[write_i + 0] = qBound(0, int(src[read_i + 0] * 255.0f), 255);
        dst[write_i + 1] = qBound(0, int(src[read_i + 1] * 255.0f), 255);
        dst[write_i + 2] = qBound(0, int(src[read_i + 2] * 255.0f), 255);
        dst[write_i + 3] = qBound(0, int(src[read_i + 3] * 255.0f), 255);
        write_i += 4;
        read_i += 4;
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to RGBA8
      buf.resize(N * 4);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint8_t gray = qBound(0, int(src[i] * 255.0f), 255);
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 255;
      }
      break;
    }

    case QRhiTexture::R32F:
    {
      // R32F to RGBA8
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint8_t gray = qBound(0, int(src[i] * 255.0f), 255);
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 255;
      }
      buf.resize(N * 4);
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to RGBA8
      buf.resize(N * 4);
      auto data = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        auto gray = data[i];
        data[i * 4 + 0] = gray;
        data[i * 4 + 1] = gray;
        data[i * 4 + 2] = gray;
        data[i * 4 + 3] = 255;
      }
      break;
    }

    case QRhiTexture::R32UI:
    {
      // R32UI to RGBA8
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint8_t gray = (uint64_t(src[i]) * 255) / 4294967295UL;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 255;
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to RGBA8
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      int write_i = 0;
      int read_i = 0;
      for(int i = 0; i < N; i++)
      {
        dst[write_i + 0] = src[read_i + 0] > 255 ? 255 : src[read_i + 0];
        dst[write_i + 1] = src[read_i + 1] > 255 ? 255 : src[read_i + 1];
        dst[write_i + 2] = src[read_i + 2] > 255 ? 255 : src[read_i + 2];
        dst[write_i + 3] = src[read_i + 3] > 255 ? 255 : src[read_i + 3];
        write_i += 4;
        read_i += 4;
      }
      buf.resize(N * 4);
      break;
    }
#endif

    default:
      if(buf.size() < N * 4)
        buf.resize(N * 4);
      break;
  }
}

inline void toR8(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
    case QRhiTexture::R8:
    case QRhiTexture::RED_OR_ALPHA8:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
#endif
      break;

    case QRhiTexture::RGBA8:
    {
      // RGBA8 to R8
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = (src[i * 4 + 0] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 2] * 114) / 1000;
        // Alternatives:
        // average: dst[i] = (src[i * 4 + 0] + src[i * 4 + 1] + src[i * 4 + 2]) / 3;
        // R channel: dst[i] = src[i * 4 + 0];
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA8 to R8
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = (src[i * 4 + 2] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 0] * 114) / 1000;
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::R16:
    {
      // R16 to R8
      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = (uint32_t(src[i]) * 255) / 65535;
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to R8
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        float gray = (src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f);
        dst[i] = qBound(0, int(gray * 255.0f), 255);
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::RGBA32F:
    {
      // RGBA32F to R8
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        float gray = (src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f);
        dst[i] = qBound(0, int(gray * 255.0f), 255);
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to R8
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = qBound(0, int(src[i] * 255.0f), 255);
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::R32F:
    {
      // R32F to R8
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = qBound(0, int(src[i] * 255.0f), 255);
      }
      buf.resize(N);
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R32UI:
    {
      // R32UI to R8
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = (uint64_t(src[i]) * 255) / 4294967295UL;
      }
      buf.resize(N);
      break;
    }

    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to R8
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint8_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint32_t gray = (src[i * 4 + 0] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 2] * 114) / 1000;
        dst[i] = gray > 255 ? 255 : gray;
      }
      buf.resize(N);
      break;
    }
#endif

    default:
      if(buf.size() > N)
        buf.resize(N);
      break;
  }
}
inline void toRGBA32F(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
    case QRhiTexture::RGBA32F:
      break;

    case QRhiTexture::RGBA8:
    {
      // RGBA8 to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i * 4 + 0] = src[i * 4 + 0] / 255.0f;
        dst[i * 4 + 1] = src[i * 4 + 1] / 255.0f;
        dst[i * 4 + 2] = src[i * 4 + 2] / 255.0f;
        dst[i * 4 + 3] = src[i * 4 + 3] / 255.0f;
      }
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA8 to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i * 4 + 0] = src[i * 4 + 2] / 255.0f; // B -> R
        dst[i * 4 + 1] = src[i * 4 + 1] / 255.0f; // G -> G
        dst[i * 4 + 2] = src[i * 4 + 0] / 255.0f; // R -> B
        dst[i * 4 + 3] = src[i * 4 + 3] / 255.0f; // A -> A
      }
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
      // R8 to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float gray = src[i] / 255.0f;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 1.0f;
      }
      break;
    }

    case QRhiTexture::R16:
    {
      // R16 to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float gray = src[i] / 65535.0f;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 1.0f;
      }
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i * 4 + 0] = src[i * 4 + 0];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 2];
        dst[i * 4 + 3] = src[i * 4 + 3];
      }
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float gray = src[i];
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 1.0f;
      }
      break;
    }

    case QRhiTexture::R32F:
    {
      // R32F to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float gray = src[i];
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 1.0f;
      }
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float gray = src[i] / 255.0f;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 1.0f;
      }
      break;
    }

    case QRhiTexture::R32UI:
    {
      // R32UI to RGBA32F
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float gray = src[i] / 4294967295.0f;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 1.0f;
      }
      break;
    }

    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to RGBA32F
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i * 4 + 0] = src[i * 4 + 0] / 4294967295.0f;
        dst[i * 4 + 1] = src[i * 4 + 1] / 4294967295.0f;
        dst[i * 4 + 2] = src[i * 4 + 2] / 4294967295.0f;
        dst[i * 4 + 3] = src[i * 4 + 3] / 4294967295.0f;
      }
      break;
    }
#endif

    default:
      if(buf.size() < N * 16)
        buf.resize(N * 16);
      break;
  }
}
inline void toR16(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
    case QRhiTexture::R16:
      break;

    case QRhiTexture::RGBA8:
    {
      // RGBA8 to R16
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        // Convert to grayscale using luminance formula
        uint32_t gray = (src[i * 4 + 0] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 2] * 114) / 1000;
        dst[i] = (gray * 65535) / 255;
      }
      buf.resize(N * 2);
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA8 to R16
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint32_t gray = (src[i * 4 + 2] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 0] * 114) / 1000;
        dst[i] = (gray * 65535) / 255;
      }
      buf.resize(N * 2);
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
      // R8 to R16
      buf.resize(N * 2);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = (src[i] * 65535) / 255;
      }
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to R16
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        float gray = (src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f);
        dst[i] = qBound(0, int(gray * 65535.0f), 65535);
      }
      buf.resize(N * 2);
      break;
    }

    case QRhiTexture::RGBA32F:
    {
      // RGBA32F to R16
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        // Grayscale from RGBA
        float gray = (src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f);
        dst[i] = qBound(0, int(gray * 65535.0f), 65535);
      }
      buf.resize(N * 2);
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to R16
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = qBound(0, int(src[i] * 65535.0f), 65535);
      }
      break;
    }

    case QRhiTexture::R32F:
    {
      // R32F to R16
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = qBound(0, int(src[i] * 65535.0f), 65535);
      }
      buf.resize(N * 2);
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to R16
      buf.resize(N * 2);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = (src[i] * 65535) / 255;
      }
      break;
    }

    case QRhiTexture::R32UI:
    {
      // R32UI to R16
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
         dst[i] = (uint64_t(src[i]) * 65535) / 4294967295UL;
      }
      buf.resize(N * 2);
      break;
    }

    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to R16
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint16_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint64_t gray = (uint64_t(src[i * 4 + 0]) * 299 + uint64_t(src[i * 4 + 1]) * 587 + uint64_t(src[i * 4 + 2]) * 114) / 1000;
        dst[i] = gray > 65535 ? 65535 : gray;
      }
      buf.resize(N * 2);
      break;
    }
#endif

    default:
      if(buf.size() != N * 2)
        buf.resize(N * 2);
      break;
  }
}
inline void toR32F(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
    case QRhiTexture::R32F:
      break;

    case QRhiTexture::RGBA8:
    {
      // RGBA8 to R32F
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint32_t gray = (src[i * 4 + 0] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 2] * 114) / 1000;
        dst[i] = gray / 255.0f;
      }
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA8 to R32F
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        // Convert to grayscale (B, G, R, A)
        uint32_t gray = (src[i * 4 + 2] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 0] * 114) / 1000;
        dst[i] = gray / 255.0f;
      }
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
      // R8 to R32F
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = src[i] / 255.0f;
      }
      break;
    }

    case QRhiTexture::R16:
    {
      // R16 to R32F
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = src[i] / 65535.0f;
      }
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to R32F
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f;
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::RGBA32F:
    {
      // RGBA32F to R32F
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        // Grayscale from RGBA
        dst[i] = src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f;
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to R32F
      buf.resize(N * 4);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = src[i];
      }
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to R32F
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = src[i] / 255.0f;
      }
      break;
    }

    case QRhiTexture::R32UI:
    {
      // R32UI to R32F
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = src[i] / 4294967295.0f;
      }
      break;
    }

    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to R32F
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<float*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        float gray = (src[i * 4 + 0] / 4294967295.0f) * 0.299f +
                     (src[i * 4 + 1] / 4294967295.0f) * 0.587f +
                     (src[i * 4 + 2] / 4294967295.0f) * 0.114f;
        dst[i] = gray;
      }
      buf.resize(N * 4);
      break;
    }
#endif

    default:
      if(buf.size() != N * 4)
        buf.resize(N * 4);
      break;
  }
}
inline void toR32UI(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R32UI:
      break;
#endif

    case QRhiTexture::RGBA8:
    {
      // RGBA8 to R32UI
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint32_t gray = (src[i * 4 + 0] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 2] * 114) / 1000;
        dst[i] = (gray * 4294967295UL) / 255;
      }
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA8 to R32UI
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint32_t gray = (src[i * 4 + 2] * 299 + src[i * 4 + 1] * 587 + src[i * 4 + 0] * 114) / 1000;
        dst[i] = (gray * 4294967295UL) / 255;
      }
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
      // R8 to R32UI
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = (src[i] * 4294967295UL) / 255;
      }
      break;
    }

    case QRhiTexture::R16:
    {
      // R16 to R32UI
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = (uint64_t(src[i]) * 4294967295UL) / 65535;
      }
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to R32UI
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        // Grayscale from RGBA
        float gray = src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f;
        dst[i] = qBound(0.0f, gray * 4294967295.0f, 4294967295.0f);
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::RGBA32F:
    {
      // RGBA32F to R32UI
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        // Grayscale from RGBA
        float gray = src[i * 4 + 0] * 0.299f + src[i * 4 + 1] * 0.587f + src[i * 4 + 2] * 0.114f;
        dst[i] = qBound(0.0f, gray * 4294967295.0f, 4294967295.0f);
      }
      buf.resize(N * 4);
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to R32UI
      buf.resize(N * 4);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        float val = float(src[i]);
        dst[i] = qBound(0.0f, val * 4294967295.0f, 4294967295.0f);
      }
      break;
    }

    case QRhiTexture::R32F:
    {
      // R32F to R32UI
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i] = qBound(0.0f, src[i] * 4294967295.0f, 4294967295.0f);
      }
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to R32UI
      buf.resize(N * 4);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i] = (src[i] * 4294967295UL) / 255;
      }
      break;
    }

    case QRhiTexture::RGBA32UI:
    {
      // RGBA32UI to R32U
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        uint64_t gray = (uint64_t(src[i * 4 + 0]) * 299 + uint64_t(src[i * 4 + 1]) * 587 + uint64_t(src[i * 4 + 2]) * 114) / 1000;
        dst[i] = gray > 4294967295UL ? 4294967295UL : gray;
      }
      buf.resize(N * 4);
      break;
    }
#endif

    default:
      if(buf.size() != N * 4)
        buf.resize(N * 4);
      break;
  }
}
inline void toRGBA32UI(QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  const int N = width * height;
  switch(in_format)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::RGBA32UI:
      break;
#endif

    case QRhiTexture::RGBA8:
    {
      // RGBA8 to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i * 4 + 0] = (src[i * 4 + 0] * 4294967295UL) / 255;
        dst[i * 4 + 1] = (src[i * 4 + 1] * 4294967295UL) / 255;
        dst[i * 4 + 2] = (src[i * 4 + 2] * 4294967295UL) / 255;
        dst[i * 4 + 3] = (src[i * 4 + 3] * 4294967295UL) / 255;
      }
      break;
    }

    case QRhiTexture::BGRA8:
    {
      // BGRA8 to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i * 4 + 0] = (src[i * 4 + 2] * 4294967295UL) / 255; // B -> R
        dst[i * 4 + 1] = (src[i * 4 + 1] * 4294967295UL) / 255; // G -> G
        dst[i * 4 + 2] = (src[i * 4 + 0] * 4294967295UL) / 255; // R -> B
        dst[i * 4 + 3] = (src[i * 4 + 3] * 4294967295UL) / 255; // A -> A
      }
      break;
    }

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
    {
      // R8 to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint32_t gray = (src[i] * 4294967295UL) / 255;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 4294967295UL;
      }
      break;
    }

    case QRhiTexture::R16:
    {
      // R16 to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint16_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint32_t gray = (uint64_t(src[i]) * 4294967295UL) / 65535;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 4294967295UL;
      }
      break;
    }

    case QRhiTexture::RGBA16F:
    {
      // RGBA16F to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        dst[i * 4 + 0] = qBound(0.0f, float(src[i * 4 + 0]) * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 1] = qBound(0.0f, float(src[i * 4 + 1]) * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 2] = qBound(0.0f, float(src[i * 4 + 2]) * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 3] = qBound(0.0f, float(src[i * 4 + 3]) * 4294967295.0f, 4294967295.0f);
      }
      break;
    }

    case QRhiTexture::RGBA32F:
    {
      // RGBA32F to RGBA32UI
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = 0; i < N; i++)
      {
        dst[i * 4 + 0] = qBound(0.0f, src[i * 4 + 0] * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 1] = qBound(0.0f, src[i * 4 + 1] * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 2] = qBound(0.0f, src[i * 4 + 2] * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 3] = qBound(0.0f, src[i * 4 + 3] * 4294967295.0f, 4294967295.0f);
      }
      break;
    }

    case QRhiTexture::R16F:
    {
      // R16F to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const qfloat16*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint32_t gray = qBound(0.0f, float(src[i]) * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 4294967295UL;
      }
      break;
    }

    case QRhiTexture::R32F:
    {
      // R32F to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const float*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint32_t gray = qBound(0.0f, src[i] * 4294967295.0f, 4294967295.0f);
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 4294967295UL;
      }
      break;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
    {
      // R8UI to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint8_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint32_t gray = (src[i] * 4294967295UL) / 255;
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 4294967295UL;
      }
      break;
    }

    case QRhiTexture::R32UI:
    {
      // R32UI to RGBA32UI
      buf.resize(N * 16);
      auto src = reinterpret_cast<const uint32_t*>(buf.constData());
      auto dst = reinterpret_cast<uint32_t*>(buf.data());
      for(int i = N - 1; i >= 0; i--)
      {
        uint32_t gray = src[i];
        dst[i * 4 + 0] = gray;
        dst[i * 4 + 1] = gray;
        dst[i * 4 + 2] = gray;
        dst[i * 4 + 3] = 4294967295UL;
      }
      break;
    }
#endif

    default:
      if(buf.size() < N * 16)
        buf.resize(N * 16);
      break;
  }
}
inline void convertTexture(QRhiTexture::Format out_format, QRhiTexture::Format in_format, QByteArray& buf, int width, int height)
{
  switch(out_format)
  {
    case QRhiTexture::BGRA8: // TODO
    case QRhiTexture::RGBA8:
      return toRGBA(in_format, buf, width, height);

    case QRhiTexture::RED_OR_ALPHA8:
    case QRhiTexture::R8:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R8UI:
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    case QRhiTexture::R8SI:
#endif
      return toR8(in_format, buf, width, height);

    case QRhiTexture::R16:
      return toR16(in_format, buf, width, height);

    case QRhiTexture::RGBA32F:
      return toRGBA32F(in_format, buf, width, height);

    case QRhiTexture::R32F:
      return toR32F(in_format, buf, width, height);

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::R32UI:
      return toR32UI(in_format, buf, width, height);

    case QRhiTexture::RGBA32UI:
      return toRGBA32UI(in_format, buf, width, height);
#endif

    case QRhiTexture::RG16:
    case QRhiTexture::RGBA16F:
    case QRhiTexture::RG8:
    case QRhiTexture::R16F:
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    case QRhiTexture::RGB10A2:
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::RG32UI:
#endif
    case QRhiTexture::D16:
    case QRhiTexture::D24:
    case QRhiTexture::D24S8:
    case QRhiTexture::D32F:
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    case QRhiTexture::D32FS8:
#endif
    case QRhiTexture::UnknownFormat:
    case QRhiTexture::BC1:
    case QRhiTexture::BC2:
    case QRhiTexture::BC3:
    case QRhiTexture::BC4:
    case QRhiTexture::BC5:
    case QRhiTexture::BC6H:
    case QRhiTexture::BC7:
    case QRhiTexture::ETC2_RGB8:
    case QRhiTexture::ETC2_RGB8A1:
    case QRhiTexture::ETC2_RGBA8:
    case QRhiTexture::ASTC_4x4:
    case QRhiTexture::ASTC_5x4:
    case QRhiTexture::ASTC_5x5:
    case QRhiTexture::ASTC_6x5:
    case QRhiTexture::ASTC_6x6:
    case QRhiTexture::ASTC_8x5:
    case QRhiTexture::ASTC_8x6:
    case QRhiTexture::ASTC_8x8:
    case QRhiTexture::ASTC_10x5:
    case QRhiTexture::ASTC_10x6:
    case QRhiTexture::ASTC_10x8:
    case QRhiTexture::ASTC_10x10:
    case QRhiTexture::ASTC_12x10:
    case QRhiTexture::ASTC_12x12:
      break;
  }
}
}

namespace oscr
{

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
  auto& rb = m_readbacks[k];
  auto& buf = rb.data;
  qsizetype bytesize{};
  if_possible(bytesize = cpu_tex.bytesize())
  else if_possible(bytesize = cpu_tex.bytesize)
  else return;

  if(buf.size() < bytesize)
  {
    cpu_tex.bytes = nullptr;
  }
  else
  {
    // FIXME ARGB needs special handling too
    if constexpr(requires { std::string_view{Tex::format()}; })
    {
      constexpr std::string_view fmt = Tex::format();
      if(fmt == "rgb") {
        gpp::qrhi::toRGB(rb.format, buf, rb.pixelSize.width(), rb.pixelSize.height());
      }
      else
      {
        const auto dstFormat = gpp::qrhi::textureFormat(cpu_tex);
        gpp::qrhi::convertTexture(dstFormat, rb.format, buf, rb.pixelSize.width(), rb.pixelSize.height());
      }
    }
    else if constexpr(requires { Tex::RGB; })
    {
      if constexpr(avnd::cpu_dynamic_format_texture<Tex>){
        if(cpu_tex.format == Tex::RGB)
          gpp::qrhi::toRGB(rb.format, buf, rb.pixelSize.width(), rb.pixelSize.height());
        else
        {
          const auto dstFormat = gpp::qrhi::textureFormat(cpu_tex);
          gpp::qrhi::convertTexture(dstFormat, rb.format, buf, rb.pixelSize.width(), rb.pixelSize.height());
        }
      }
      else
      {
        gpp::qrhi::toRGB(rb.format, buf, rb.pixelSize.width(), rb.pixelSize.height());
      }
    }
    else
    {
      const auto dstFormat = gpp::qrhi::textureFormat(cpu_tex);
      gpp::qrhi::convertTexture(dstFormat, rb.format, buf, rb.pixelSize.width(), rb.pixelSize.height());
    }

    using components_type = std::decay_t<decltype(cpu_tex.bytes)>;
    cpu_tex.bytes = reinterpret_cast<components_type>(buf.data());

    if(rhi.isYUpInNDC())
      if(cpu_tex.width * cpu_tex.height > 0)
      {
        int bpp = 0;
        if constexpr(requires { cpu_tex.bytes_per_pixel(); })
          bpp = cpu_tex.bytes_per_pixel();
        else
          bpp = cpu_tex.bytes_per_pixel;

        inplaceMirror(
            reinterpret_cast<unsigned char*>(cpu_tex.bytes), cpu_tex.width,
            cpu_tex.height, bpp);
      }

    cpu_tex.changed = true;
  }
}
}
