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
class BaseScenario;
namespace RecreateOnPlay
{

class BaseScenarioElement;

class DocumentPlugin final : public iscore::DocumentPluginModel
{
    public:
        DocumentPlugin(iscore::Document& doc, QObject* parent);

        void reload(BaseScenario& doc);
        void clear();

        BaseScenarioElement* baseScenario() const;

    private:
        BaseScenarioElement* m_base{};
};
}
