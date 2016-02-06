#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <QString>
#include <vector>

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore
struct VisitorVariant;

// TODO rename file
// TODO DocumentPlugin -> system
namespace iscore
{
class ISCORE_LIB_BASE_EXPORT DocumentPlugin :
        public NamedObject
{
        Q_OBJECT
    public:
        DocumentPlugin(
                const iscore::DocumentContext&,
                const QString& name,
                QObject* parent);

        virtual ~DocumentPlugin();

        virtual std::vector<ElementPluginModelType> elementPlugins() const { return {}; }
        virtual ElementPluginModel* makeElementPlugin(
                const QObject* element,
                ElementPluginModelType type,
                QObject* parent) { return nullptr; }
        virtual ElementPluginModel* loadElementPlugin(
                const QObject* element,
                const VisitorVariant&,
                QObject* parent) { return nullptr; }

        virtual ElementPluginModel* cloneElementPlugin(
                const QObject* element,
                iscore::ElementPluginModel* source,
                QObject* parent) { return nullptr; }

        virtual QWidget* makeElementPluginWidget(
                const ElementPluginModel*,
                QWidget* parent) const { return nullptr; }

        auto& context() const
        { return m_context; }

    protected:
        const iscore::DocumentContext& m_context;
};


class ISCORE_LIB_BASE_EXPORT SerializableDocumentPlugin :
        public DocumentPlugin,
        public SerializableInterface<DocumentPlugin>
{
        //ISCORE_SERIALIZE_FRIENDS(SerializableDocumentPlugin, DataStream)
        //ISCORE_SERIALIZE_FRIENDS(SerializableDocumentPlugin, JSONObject)

    protected:
        using DocumentPlugin::DocumentPlugin;
        using ConcreteFactoryKey = UuidKey<DocumentPlugin>;

    virtual ~SerializableDocumentPlugin();
};


class ISCORE_LIB_BASE_EXPORT DocumentPluginFactory :
        public iscore::GenericFactoryInterface<UuidKey<DocumentPlugin>>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                DocumentPlugin,
                "570faa0b-f100-4039-a2f0-b60347c4e581")
    public:
        virtual ~DocumentPluginFactory();

        virtual DocumentPlugin* load(
                const VisitorVariant& var,
                iscore::DocumentContext& doc,
                QObject* parent) = 0;

};
class ISCORE_LIB_BASE_EXPORT DocumentPluginFactoryList final :
        public iscore::ConcreteFactoryList<iscore::DocumentPluginFactory>
{
};

}


