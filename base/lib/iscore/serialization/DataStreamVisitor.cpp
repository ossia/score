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
