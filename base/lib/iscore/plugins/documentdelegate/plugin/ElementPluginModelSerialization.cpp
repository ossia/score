#include "ElementPluginModel.hpp"
#include "ElementPluginModelSerialization.hpp"


// TODO do the same for DocumentDelegateModel...
template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::ElementPluginModel& elt)
{
    m_stream << elt.plugin();
    elt.serialize(this->toVariant());

    insertDelimiter();
}

template<>
void Visitor<Reader<JSON>>::readFrom(const iscore::ElementPluginModel& elt)
{
    m_obj["Name"] = elt.plugin();
    elt.serialize(this->toVariant());
}

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

template<>
iscore::ElementPluginModel* deserializeElementPluginModel(
        Deserializer<DataStream>& deserializer,
        const QString& elementName,
        QObject* parent)
{
    QString savedName;
    deserializer.m_stream >> savedName;

    iscore::Document* doc = iscore::IDocument::documentFromObject(parent);

    iscore::ElementPluginModel* model{};
    for(auto& plugin : doc->model()->pluginModels())
    {
        if(plugin->metadataName() == savedName)
        {
            model = plugin->makeElementPlugin(
                        elementName,
                        deserializer.toVariant(),
                        parent);
            break;
        }
    }

    deserializer.checkDelimiter();
    return model;
}


template<>
iscore::ElementPluginModel* deserializeElementPluginModel(
        Deserializer<JSON>& deserializer,
        const QString& elementName,
        QObject* parent)
{
    QString savedName = deserializer.m_obj["Name"].toString();

    iscore::Document* doc = iscore::IDocument::documentFromObject(parent);

    iscore::ElementPluginModel* model{};
    for(auto& plugin : doc->model()->pluginModels())
    {
        if(plugin->metadataName() == savedName)
        {
            model = plugin->makeElementPlugin(
                        elementName,
                        deserializer.toVariant(),
                        parent);
            break;
        }
    }

    return model;
}

