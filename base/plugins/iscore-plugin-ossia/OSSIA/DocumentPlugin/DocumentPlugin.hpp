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

namespace RecreateOnPlay
{

class BaseScenarioElement;

class DocumentPlugin final : public iscore::DocumentPluginModel
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
