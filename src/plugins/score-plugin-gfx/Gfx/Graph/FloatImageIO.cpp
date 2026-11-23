#include <Gfx/Graph/FloatImageIO.hpp>

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QtEndian>

#include <cmath>
#include <cstring>

namespace score::gfx
{

namespace
{
bool pfmIsSpace(char c)
{
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

bool pfmReadToken(const QByteArray& b, qsizetype& pos, QByteArray& tok)
{
  while(pos < b.size() && pfmIsSpace(b[pos]))
    pos++;
  if(pos < b.size() && b[pos] == '#')
  {
    while(pos < b.size() && b[pos] != '\n')
      pos++;
    return pfmReadToken(b, pos, tok);
  }
  const qsizetype start = pos;
  while(pos < b.size() && !pfmIsSpace(b[pos]))
    pos++;
  tok = b.mid(start, pos - start);
  return !tok.isEmpty();
}
}

FloatImage parsePFM(const QByteArray& b, QString* error)
{
  FloatImage img;
  qsizetype pos = 0;
  QByteArray magic, w, h, scale;
  if(!pfmReadToken(b, pos, magic) || !pfmReadToken(b, pos, w) || !pfmReadToken(b, pos, h)
     || !pfmReadToken(b, pos, scale))
  {
    setFileError(error, QStringLiteral("truncated PFM header"));
    return {};
  }
  if(magic == "PF")
    img.channels = 3;
  else if(magic == "Pf")
    img.channels = 1;
  else
  {
    setFileError(error, QStringLiteral("not a PFM file"));
    return {};
  }

  bool okW{}, okH{}, okS{};
  img.width = w.toInt(&okW);
  img.height = h.toInt(&okH);
  const double s = scale.toDouble(&okS);
  if(!okW || !okH || !okS || img.width <= 0 || img.height <= 0 || s == 0.
     || img.width > 65536 || img.height > 65536)
  {
    setFileError(error, QStringLiteral("invalid PFM header"));
    return {};
  }
  if(pos + 1 < b.size() && b[pos] == '\r' && b[pos + 1] == '\n')
    pos++;
  if(pos >= b.size() || !pfmIsSpace(b[pos]))
  {
    setFileError(error, QStringLiteral("truncated PFM data"));
    return {};
  }
  pos++;

  const bool littleEndian = s < 0.;
  const std::size_t count = std::size_t(img.width) * img.height * img.channels;
  if(std::size_t(b.size() - pos) / sizeof(float) < count)
  {
    setFileError(error, QStringLiteral("truncated PFM data"));
    return {};
  }

  img.data.resize(count);
  const std::size_t rowFloats = std::size_t(img.width) * img.channels;
  const char* src = b.constData() + pos;
  for(int y = 0; y < img.height; y++)
  {
    float* dst = img.row(img.height - 1 - y);
    const char* s = src + y * rowFloats * sizeof(float);
    for(std::size_t i = 0; i < rowFloats; i++)
    {
      quint32 u;
      std::memcpy(&u, s + i * sizeof(float), sizeof(u));
      u = littleEndian ? qFromLittleEndian(u) : qFromBigEndian(u);
      std::memcpy(dst + i, &u, sizeof(u));
    }
  }
  return img;
}

FloatImage readPFM(const QString& path, QString* error)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
  {
    setFileError(error, QStringLiteral("cannot open %1").arg(path));
    return {};
  }
  return parsePFM(f.readAll(), error);
}

QByteArray encodePFM(const FloatImage& img)
{
  if(!img.valid() || (img.channels != 1 && img.channels != 3))
    return {};
  QByteArray out = QByteArray(img.channels == 3 ? "PF\n" : "Pf\n")
                   + QByteArray::number(img.width) + ' '
                   + QByteArray::number(img.height) + "\n-1.0\n";
  const std::size_t rowFloats = std::size_t(img.width) * img.channels;
  const qsizetype header = out.size();
  out.resize(header + qsizetype(rowFloats * img.height * sizeof(float)));
  char* dst = out.data() + header;
  for(int y = 0; y < img.height; y++)
  {
    const float* src = img.row(img.height - 1 - y);
    for(std::size_t i = 0; i < rowFloats; i++)
    {
      quint32 u;
      std::memcpy(&u, src + i, sizeof(u));
      u = qToLittleEndian(u);
      std::memcpy(dst, &u, sizeof(u));
      dst += sizeof(u);
    }
  }
  return out;
}

bool writePFM(const QString& path, const FloatImage& img, QString* error)
{
  const QByteArray bytes = encodePFM(img);
  if(bytes.isEmpty())
  {
    setFileError(error, QStringLiteral("a PFM holds 1 or 3 channels"));
    return false;
  }
  return writeFileAtomic(path, bytes, error);
}

bool writeFileAtomic(const QString& path, const QByteArray& bytes, QString* error)
{
  QSaveFile f{path};
  if(!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit())
  {
    setFileError(error, QStringLiteral("cannot write %1").arg(path));
    return false;
  }
  return true;
}

QString validMapPath(const QString& pfmPath)
{
  QString base = pfmPath;
  if(base.endsWith(QStringLiteral(".pfm"), Qt::CaseInsensitive))
    base.chop(4);
  if(base.endsWith(QStringLiteral(".xyz"), Qt::CaseInsensitive))
    base.chop(4);
  return base + QStringLiteral(".valid.pfm");
}

FloatImage toRGBA(const FloatImage& img)
{
  if(!img.valid())
    return {};
  if(img.channels == 4)
    return img;
  FloatImage out;
  out.width = img.width;
  out.height = img.height;
  out.channels = 4;
  const std::size_t n = std::size_t(img.width) * img.height;
  out.data.resize(n * 4);
  for(std::size_t i = 0; i < n; i++)
  {
    float* d = out.data.data() + i * 4;
    const float* s = img.data.data() + i * img.channels;
    switch(img.channels)
    {
      case 1:
        d[0] = d[1] = d[2] = s[0];
        d[3] = 1.f;
        break;
      case 2:
        d[0] = s[0];
        d[1] = s[1];
        d[2] = 0.f;
        d[3] = 1.f;
        break;
      default:
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        d[3] = 1.f;
        break;
    }
  }
  return out;
}

FloatImage loadFloatImageRGBA(const QString& path, QString* error)
{
  if(path.endsWith(QStringLiteral(".pfm"), Qt::CaseInsensitive))
  {
    FloatImage img = readPFM(path, error);
    if(!img.valid())
      return {};
    FloatImage rgba = toRGBA(img);
    if(img.channels == 3)
    {
      const QString validPath = validMapPath(path);
      if(validPath != path && QFileInfo::exists(validPath))
      {
        const FloatImage valid = readPFM(validPath);
        if(valid.valid() && valid.width == img.width && valid.height == img.height)
        {
          const std::size_t n = std::size_t(img.width) * img.height;
          for(std::size_t i = 0; i < n; i++)
            rgba.data[i * 4 + 3] = valid.data[i * valid.channels];
        }
      }
    }
    return rgba;
  }

  QImage qimg{path};
  if(qimg.isNull())
  {
    setFileError(error, QStringLiteral("cannot read %1").arg(path));
    return {};
  }
  return toFloatImageRGBA(qimg);
}

FloatImage toFloatImageRGBA(const QImage& src)
{
  if(src.isNull())
    return {};
  const QImage qimg = src.convertToFormat(QImage::Format_RGBA32FPx4);
  FloatImage out;
  out.width = qimg.width();
  out.height = qimg.height();
  out.channels = 4;
  out.data.resize(std::size_t(out.width) * out.height * 4);
  for(int y = 0; y < out.height; y++)
    std::memcpy(out.row(y), qimg.constScanLine(y), std::size_t(out.width) * 4 * sizeof(float));
  return out;
}

bool writeFloatImageSplit(const QString& basePath, const FloatImage& img, QString* error)
{
  if(!img.valid())
  {
    setFileError(error, QStringLiteral("empty image"));
    return false;
  }
  if(img.channels == 1 || img.channels == 3)
    return writePFM(basePath + QStringLiteral(".pfm"), img, error);

  const FloatImage rgba = toRGBA(img);
  FloatImage xyz, valid;
  xyz.width = valid.width = rgba.width;
  xyz.height = valid.height = rgba.height;
  xyz.channels = 3;
  valid.channels = 1;
  const std::size_t n = std::size_t(rgba.width) * rgba.height;
  xyz.data.resize(n * 3);
  valid.data.resize(n);
  for(std::size_t i = 0; i < n; i++)
  {
    const float* s = rgba.data.data() + i * 4;
    xyz.data[i * 3 + 0] = s[0];
    xyz.data[i * 3 + 1] = s[1];
    xyz.data[i * 3 + 2] = s[2];
    valid.data[i] = s[3];
  }
  return writePFM(basePath + QStringLiteral(".xyz.pfm"), xyz, error)
         && writePFM(basePath + QStringLiteral(".valid.pfm"), valid, error);
}
}
