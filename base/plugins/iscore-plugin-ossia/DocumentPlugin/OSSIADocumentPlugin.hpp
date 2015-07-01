#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
namespace iscore
{
class DocumentModel;
}

class OSSIABaseScenarioElement;
class OSSIADocumentPlugin : public iscore::DocumentDelegatePluginModel
{
    public:
        OSSIADocumentPlugin(iscore::DocumentModel* doc, QObject* parent);

        OSSIABaseScenarioElement* baseScenario() const;

        QList<iscore::ElementPluginModelType> elementPlugins() const override;
        iscore::ElementPluginModel*makeElementPlugin(
                const QObject* element,
                iscore::ElementPluginModelType type,
                QObject* parent) override;
        iscore::ElementPluginModel*loadElementPlugin(
                const QObject* element,
                const VisitorVariant&,
                QObject* parent) override;
        iscore::ElementPluginModel* cloneElementPlugin(
                const QObject* element,
                iscore::ElementPluginModel* source,
                QObject* parent) override;
        QWidget* makeElementPluginWidget(
                const iscore::ElementPluginModel*,
                QWidget* parent) const override;
        void serialize(const VisitorVariant&) const override;

    private:
        OSSIABaseScenarioElement* m_base{};
};
