
#include <QIODevice>

#include "DataStreamVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

Visitor<Reader<DataStream>>::Visitor():
    context{iscore::AppContext()}
{

}

Visitor<Reader<DataStream>>::Visitor(QByteArray* array):
    m_stream_impl{array, QIODevice::WriteOnly},
    context{iscore::AppContext()}
{
    m_stream_impl.setVersion(QDataStream::Qt_5_3);
}

Visitor<Reader<DataStream>>::Visitor(QIODevice* dev):
    m_stream_impl {dev},
    context{iscore::AppContext()}
{
}




Visitor<Writer<DataStream>>::Visitor():
    context{iscore::AppContext()}
{

}

Visitor<Writer<DataStream>>::Visitor(const QByteArray& array):
    m_stream_impl{array},
    context{iscore::AppContext()}
{
    m_stream_impl.setVersion(QDataStream::Qt_5_3);
}

Visitor<Writer<DataStream>>::Visitor(QIODevice* dev):
    m_stream_impl{dev},
    context{iscore::AppContext()}
{
}
