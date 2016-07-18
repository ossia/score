#pragma once
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <QString>
#include <vector>

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

        auto& context() const
        { return m_context; }

    protected:
        const iscore::DocumentContext& m_context;
};

class DocumentPluginFactory;
class ISCORE_LIB_BASE_EXPORT SerializableDocumentPlugin :
        public DocumentPlugin,
        public SerializableInterface<DocumentPluginFactory>
{
    protected:
        using DocumentPlugin::DocumentPlugin;
        using ConcreteFactoryKey = UuidKey<DocumentPluginFactory>;

    virtual ~SerializableDocumentPlugin();
};


class ISCORE_LIB_BASE_EXPORT DocumentPluginFactory :
        public iscore::AbstractFactory<DocumentPluginFactory>
{
        ISCORE_ABSTRACT_FACTORY("570faa0b-f100-4039-a2f0-b60347c4e581")
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
    public:
        using object_type = DocumentPlugin;
};

}
