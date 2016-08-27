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
inline constexpr const char * addressettings() { return "application/x-iscore-address-settings"; }
}
}


template<>
struct Visitor<Reader<Mime<Device::FullAddressSettings>>> : public MimeDataReader
{
        using MimeDataReader::MimeDataReader;
        void serialize(const Device::FullAddressSettings& lst) const
        {
            m_mime.setData(
                        iscore::mime::addressettings(),
                        QJsonDocument(toJsonObject(lst)).toJson(QJsonDocument::Indented));
        }
};

template<>
struct Visitor<Writer<Mime<Device::FullAddressSettings>>> : public MimeDataWriter
{
        using MimeDataWriter::MimeDataWriter;
        auto deserialize()
        {
            auto obj = QJsonDocument::fromJson(m_mime.data(iscore::mime::addressettings())).object();
            return fromJsonObject<Device::FullAddressSettings>(obj);
        }
};


template<>
struct Visitor<Reader<Mime<Device::NodeList>>> : public MimeDataReader
{
        using MimeDataReader::MimeDataReader;
        void serialize(const Device::NodeList& lst) const
        {
            QJsonArray arr;

            for(const auto& elt : lst)
            {
                auto node_obj = toJsonObject(*elt);
                node_obj.insert("Address", toJsonObject(Device::address(*elt)));
                arr.append(std::move(node_obj));
            }

            m_mime.setData(
                        iscore::mime::nodelist(),
                        QJsonDocument(std::move(arr)).toJson(QJsonDocument::Indented));
        }
};

template<>
struct Visitor<Writer<Mime<Device::FreeNodeList>>> : public MimeDataWriter
{
        using MimeDataWriter::MimeDataWriter;
        auto deserialize()
        {
            Device::FreeNodeList ml;
            auto arr =  QJsonDocument::fromJson(m_mime.data(iscore::mime::nodelist())).array();

            auto& strings = iscore::StringConstant();
            for(const auto& elt : arr)
            {
                Device::FreeNode n;
                auto obj = elt.toObject();
                n.first = fromJsonObject<State::Address>(obj[strings.Address]);

                Deserializer<JSONObject> des{obj};
                des.writeTo(n.second);
                ml.push_back(n);
            }

            return ml;
        }
};
