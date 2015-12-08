#pragma once

#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include "Model/CSPScenario.hpp"

namespace iscore
{
class DocumentModel;
}

class OSSIABaseScenarioElement;
class CSPDocumentPlugin : public iscore::DocumentPluginModel
{
        Q_OBJECT
    public:
        CSPDocumentPlugin(iscore::Document& doc, QObject* parent);

        void reload(iscore::DocumentModel& doc);

        CSPScenario* getScenario() const;

    private:
        CSPScenario* m_cspScenario;
};
