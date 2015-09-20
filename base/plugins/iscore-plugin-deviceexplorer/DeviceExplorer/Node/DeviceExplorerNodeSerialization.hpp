#pragma once
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/MimeVisitor.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

namespace iscore
{
namespace mime
{
inline constexpr const char * nodelist() { return "application/x-iscore-nodelist"; }
}
}


template<>
struct Visitor<Reader<Mime<iscore::NodeList>>> : public MimeDataReader
{
        using MimeDataReader::MimeDataReader;
        void serialize(const iscore::NodeList& lst) const
        {
            m_mime.setData(
                        iscore::mime::nodelist(),
                        QJsonDocument(toJsonArray(lst)).toJson(QJsonDocument::Indented));
        }
};

template<>
struct Visitor<Writer<Mime<iscore::NodeList>>> : public MimeDataWriter
{
        using MimeDataWriter::MimeDataWriter;
        auto deserialize()
        {
            iscore::NodeList ml;
            fromJsonArray(
                        QJsonDocument::fromJson(m_mime.data(iscore::mime::nodelist())).array(),
                        ml);
            return ml;
        }
};
