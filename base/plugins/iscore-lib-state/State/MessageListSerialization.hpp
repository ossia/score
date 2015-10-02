#pragma once
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/MimeVisitor.hpp>
#include <State/Message.hpp>

namespace iscore
{
namespace mime
{
inline constexpr const char * messagelist() { return "application/x-iscore-messagelist"; }
}
}

template<>
struct Visitor<Reader<Mime<iscore::MessageList>>> : public MimeDataReader
{
        using MimeDataReader::MimeDataReader;
        void serialize(const iscore::MessageList& lst) const
        {
            m_mime.setData(
                        iscore::mime::messagelist(),
                        QJsonDocument(toJsonArray(lst)).toJson(QJsonDocument::Indented));
        }
};

template<>
struct Visitor<Writer<Mime<iscore::MessageList>>> : public MimeDataWriter
{
        using MimeDataWriter::MimeDataWriter;
        auto deserialize()
        {
            iscore::MessageList ml;
            fromJsonArray(
                        QJsonDocument::fromJson(m_mime.data(iscore::mime::messagelist())).array(),
                        ml);
            return ml;
        }
};
