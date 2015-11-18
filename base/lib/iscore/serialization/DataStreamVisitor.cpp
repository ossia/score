#include "DataStreamVisitor.hpp"
#include <core/application/Application.hpp>
Visitor<Reader<DataStream>>::Visitor():
    context{iscore::Application::instance()}
{

}

Visitor<Reader<DataStream>>::Visitor(QByteArray* array):
    m_stream_impl{array, QIODevice::WriteOnly},
    context{iscore::Application::instance()}
{
    m_stream_impl.setVersion(QDataStream::Qt_5_3);
}

Visitor<Reader<DataStream>>::Visitor(QIODevice* dev):
    m_stream_impl {dev},
    context{iscore::Application::instance()}
{
}




Visitor<Writer<DataStream>>::Visitor():
    context{iscore::Application::instance()}
{

}

Visitor<Writer<DataStream>>::Visitor(const QByteArray& array):
    m_stream_impl{array},
    context{iscore::Application::instance()}
{
    m_stream_impl.setVersion(QDataStream::Qt_5_3);
}

Visitor<Writer<DataStream>>::Visitor(QIODevice* dev):
    m_stream_impl{dev},
    context{iscore::Application::instance()}
{
}
