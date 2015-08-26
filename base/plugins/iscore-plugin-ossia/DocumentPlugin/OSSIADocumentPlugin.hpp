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
        OSSIADocumentPlugin(iscore::DocumentModel& doc, QObject* parent);

        void reload(iscore::DocumentModel& doc);

        OSSIABaseScenarioElement* baseScenario() const;

    private:
        OSSIABaseScenarioElement* m_base{};
};
