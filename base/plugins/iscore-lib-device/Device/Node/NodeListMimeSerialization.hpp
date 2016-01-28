#pragma once
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/MimeVisitor.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <QJsonDocument>

namespace iscore
{
namespace mime
{
inline constexpr const char * nodelist() { return "application/x-iscore-nodelist"; }
}
}


template<>
struct Visitor<Reader<Mime<Device::NodeList>>> : public MimeDataReader
{
        using MimeDataReader::MimeDataReader;
        void serialize(const Device::NodeList& lst) const
        {
            m_mime.setData(
                        iscore::mime::nodelist(),
                        QJsonDocument(toJsonArray(lst)).toJson(QJsonDocument::Indented));
        }
};

template<>
struct Visitor<Writer<Mime<Device::NodeList>>> : public MimeDataWriter
{
        using MimeDataWriter::MimeDataWriter;
        auto deserialize()
        {
            Device::NodeList ml;
            fromJsonArray(
                        QJsonDocument::fromJson(m_mime.data(iscore::mime::nodelist())).array(),
                        ml);
            return ml;
        }
};
