#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
namespace iscore
{
class DocumentModel;
}

namespace RecreateOnPlay
{

class BaseScenarioElement;
class DocumentPlugin final : public iscore::DocumentDelegatePluginModel
{
    public:
        DocumentPlugin(iscore::Document& doc, QObject* parent);

        void reload(iscore::DocumentModel& doc);
        void clear();

        BaseScenarioElement* baseScenario() const;

    private:
        BaseScenarioElement* m_base{};
};
}
