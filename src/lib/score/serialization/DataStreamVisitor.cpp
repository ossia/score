// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DataStreamVisitor.hpp"

#include <score/application/ApplicationContext.hpp>

#include <QIODevice>

#include <stdexcept>

DataStreamReader::DataStreamReader()
    : components{score::AppComponents()}
{
}

DataStreamReader::DataStreamReader(QByteArray* array)
    : m_stream_impl{array, QIODevice::WriteOnly}
    , components{score::AppComponents()}
{
  m_stream_impl.setVersion(QDataStream::Qt_DefaultCompiledVersion);
}

DataStreamReader::DataStreamReader(QIODevice* dev)
    : m_stream_impl{dev}
    , components{score::AppComponents()}
{
}

DataStreamWriter::DataStreamWriter()
    : components{score::AppComponents()}
{
}

DataStreamWriter::DataStreamWriter(const QByteArray& array)
    : components{score::AppComponents()}
    , m_stream_impl{array}
{
  m_stream_impl.setVersion(QDataStream::Qt_DefaultCompiledVersion);
}

DataStreamWriter::DataStreamWriter(QIODevice* dev)
    : components{score::AppComponents()}
    , m_stream_impl{dev}
{
}

bool DataStreamWriter::atDelimiterOrEnd()
{
  auto dev = m_stream_impl.device();
  if(!dev)
    return true;
  const QByteArray next = dev->peek(4);
  if(next.size() < 4)
    return true;
  QDataStream s{next};
  s.setByteOrder(m_stream_impl.byteOrder());
  int32_t val{};
  s >> val;
  return val == int32_t(0xDEADBEEF);
}

void DataStreamWriter::checkDelimiter()
{
  int val{};
  m_stream >> val;

  if(val != int32_t(0xDEADBEEF))
  {
    SCORE_BREAKPOINT;
    throw std::runtime_error("Corrupt save file.");
  }
}

QDataStream& operator<<(QDataStream& stream, const std::string& obj)
{
  uint32_t size = obj.size();
  stream << size;

  stream.writeRawData(obj.data(), size);
  return stream;
}

QDataStream& operator>>(QDataStream& stream, std::string& obj)
{
  uint32_t n = 0;
  stream >> n;
  obj.resize(n);

  char* addr = n > 0 ? &obj[0] : nullptr;
  stream.readRawData(addr, n);

  return stream;
}
