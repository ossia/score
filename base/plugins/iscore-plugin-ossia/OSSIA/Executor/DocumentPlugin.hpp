#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <memory>
#include <iscore/tools/Metadata.hpp>
#include <iscore_plugin_ossia_export.h>
#include <OSSIA/Executor/ExecutorContext.hpp>
class QObject;
namespace iscore {
class Document;
}  // namespace iscore

namespace iscore
{
class DocumentModel;
}
namespace Scenario
{
class BaseScenario;
}
namespace RecreateOnPlay
{

class BaseScenarioElement;

class ISCORE_PLUGIN_OSSIA_EXPORT DocumentPlugin final : public iscore::DocumentPlugin
{
    public:
        DocumentPlugin(iscore::Document& doc, QObject* parent);

        ~DocumentPlugin();
        void reload(Scenario::BaseScenario& doc);
        void clear();

        BaseScenarioElement* baseScenario() const;

    private:
        Context m_ctx;
        std::unique_ptr<BaseScenarioElement> m_base;
};
}
