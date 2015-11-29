#pragma once
#include <core/document/DocumentContext.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModel.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <QString>
#include <vector>

class QObject;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore
struct VisitorVariant;

namespace iscore
{
class DocumentDelegatePluginModel : public NamedObject
{
        Q_OBJECT
    public:
        DocumentDelegatePluginModel(
                iscore::Document&,
                const QString& name,
                QObject* parent);

        virtual ~DocumentDelegatePluginModel();

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

        virtual void serialize(const VisitorVariant&) const {}

        auto& context() const
        { return m_context; }

    protected:
        iscore::DocumentContext m_context;
};

}
