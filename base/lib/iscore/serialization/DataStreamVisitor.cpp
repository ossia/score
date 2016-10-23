#include "DataStreamVisitor.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <QIODevice>

template <typename T> class Reader;
template <typename T> class Writer;

Visitor<Reader<DataStream>>::Visitor():
    components{iscore::AppComponents()}
{
}

Visitor<Reader<DataStream>>::Visitor(QByteArray* array):
    m_stream_impl{array, QIODevice::WriteOnly},
    components{iscore::AppComponents()}
{
    m_stream_impl.setVersion(QDataStream::Qt_5_3);
}

Visitor<Reader<DataStream>>::Visitor(QIODevice* dev):
    m_stream_impl {dev},
    components{iscore::AppComponents()}
{
}




Visitor<Writer<DataStream>>::Visitor():
    components{iscore::AppComponents()}
{

}

Visitor<Writer<DataStream>>::Visitor(const QByteArray& array):
    m_stream_impl{array},
    components{iscore::AppComponents()}
{
    m_stream_impl.setVersion(QDataStream::Qt_5_3);
}

Visitor<Writer<DataStream>>::Visitor(QIODevice* dev):
    m_stream_impl{dev},
    components{iscore::AppComponents()}
{
}

QDataStream&operator<<(QDataStream& s, char c)
{
  return s << QChar(c);
}

QDataStream&operator>>(QDataStream& s, char& c)
{
  QChar r;
  s >> r;
  c = r.toLatin1();
  return s;
}

QDataStream&operator<<(QDataStream& stream, const std::__cxx11::string& obj)
{
  uint32_t size = obj.size();
  stream << size;

  stream.writeRawData(obj.data(), size);
  return stream;
}

QDataStream&operator>>(QDataStream& stream, std::__cxx11::string& obj)
{
  uint32_t n = 0;
  stream >> n;
  obj.resize(n);

  char* addr = n > 0 ? &obj[0] : nullptr;
  stream.readRawData(addr, n);

  return stream;
}
