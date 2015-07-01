#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
namespace iscore
{
class DocumentModel;
}

class OSSIABaseScenarioElement;
class OSSIADocumentPlugin : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        OSSIADocumentPlugin(iscore::DocumentModel* doc, QObject* parent);

        OSSIABaseScenarioElement* baseScenario() const;

        void serialize(const VisitorVariant&) const override;

    private:
        OSSIABaseScenarioElement* m_base{};
};
