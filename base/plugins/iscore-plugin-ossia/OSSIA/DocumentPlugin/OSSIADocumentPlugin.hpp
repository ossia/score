#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>

class QObject;
namespace iscore {
class Document;
}  // namespace iscore

namespace iscore
{
class DocumentModel;
}

class OSSIABaseScenarioElement;

class OSSIADocumentPlugin final : public iscore::DocumentDelegatePluginModel
{
    public:
        OSSIADocumentPlugin(iscore::Document& doc, QObject* parent);

        void reload(iscore::DocumentModel& doc);
        void clear();

        OSSIABaseScenarioElement* baseScenario() const;

    private:
        OSSIABaseScenarioElement* m_base{};
};
