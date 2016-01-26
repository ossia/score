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
class ISCORE_LIB_BASE_EXPORT DocumentPluginModel :
        public NamedObject
{
        Q_OBJECT
    public:
        DocumentPluginModel(
                iscore::Document&,
                const QString& name,
                QObject* parent);

        virtual ~DocumentPluginModel();

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
        iscore::DocumentContext m_context;
};


class ISCORE_LIB_BASE_EXPORT SerializableDocumentPluginModel :
        public DocumentPluginModel,
        public SerializableInterface<SerializableDocumentPluginModel>
{
        ISCORE_SERIALIZE_FRIENDS(SerializableDocumentPluginModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(SerializableDocumentPluginModel, JSONObject)

    protected:
        using DocumentPluginModel::DocumentPluginModel;
        using ConcreteFactoryKey = UuidKey<SerializableDocumentPluginModel>;

    virtual ~SerializableDocumentPluginModel();
};


class ISCORE_LIB_BASE_EXPORT DocumentPluginModelFactory :
        public iscore::GenericFactoryInterface<UuidKey<DocumentPluginModel>>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                DocumentPluginModel,
                "570faa0b-f100-4039-a2f0-b60347c4e581")
    public:
        virtual ~DocumentPluginModelFactory();

        virtual DocumentPluginModel* makeModel(
                const iscore::DocumentContext& doc,
                QObject* parent) = 0;

        virtual DocumentPluginModel* loadModel(
                const VisitorVariant& var,
                const iscore::DocumentContext& doc,
                QObject* parent) = 0;

};
class ISCORE_LIB_BASE_EXPORT DocumentPluginModelFactoryList final : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(iscore::DocumentPluginModelFactory)
};
}
