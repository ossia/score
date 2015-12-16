#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <memory>
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

        ~DocumentPlugin();
        void reload(BaseScenario& doc);
        void clear();

        BaseScenarioElement* baseScenario() const;

private:
        std::unique_ptr<BaseScenarioElement> m_base{};
};
}
