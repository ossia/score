#pragma once
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/MimeVisitor.hpp>
#include <Process/ProcessFactory.hpp>
#include <QJsonDocument>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
namespace iscore
{
namespace mime
{
inline constexpr auto processdata() { return "application/x-iscore-processdata"; }
}
}

namespace Process
{
struct ProcessData
{
        UuidKey<Process::ProcessModelFactory> key;
};
}
template<>
struct Visitor<Reader<Mime<Process::ProcessData>>> : public MimeDataReader
{
        using MimeDataReader::MimeDataReader;
        void serialize(const Process::ProcessData& lst) const
        {
            QJsonObject obj;
            obj["Type"] = "Process";
            obj["uuid"] = toJsonValue(lst.key.impl());
            m_mime.setData(
                        iscore::mime::processdata(),
                        QJsonDocument(obj).toJson(QJsonDocument::Indented));
        }
};

template<>
struct Visitor<Writer<Mime<Process::ProcessData>>> : public MimeDataWriter
{
        using MimeDataWriter::MimeDataWriter;
        auto deserialize()
        {
            auto obj = QJsonDocument::fromJson(m_mime.data(iscore::mime::processdata())).object();
            Process::ProcessData p;
            p.key = fromJsonValue<iscore::uuid_t>(obj["uuid"]);
            return p;
        }
};
