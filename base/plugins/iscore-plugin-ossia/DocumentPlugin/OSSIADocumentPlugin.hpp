#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
namespace iscore
{
class DocumentModel;
}

class OSSIABaseScenarioElement;
class OSSIADocumentPlugin final : public iscore::DocumentDelegatePluginModel
{
        Q_OBJECT
    public:
        OSSIADocumentPlugin(iscore::Document& doc, QObject* parent);

        void reload(iscore::DocumentModel& doc);
        void clear();

        OSSIABaseScenarioElement* baseScenario() const;

    private:
        OSSIABaseScenarioElement* m_base{};
};
