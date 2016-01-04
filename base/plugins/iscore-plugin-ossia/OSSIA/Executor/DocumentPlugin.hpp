#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <memory>
#include <iscore/tools/Metadata.hpp>
#include <iscore_plugin_ossia_export.h>
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
        ISCORE_METADATA(RecreateOnPlay::DocumentPlugin)
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
