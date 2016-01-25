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

}
