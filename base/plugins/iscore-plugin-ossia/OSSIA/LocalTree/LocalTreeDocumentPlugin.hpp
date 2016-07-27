#pragma once
#include <ossia/network/v1/Node.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/tools/Metadata.hpp>


#include "SetProperty.hpp"
#include "GetProperty.hpp"
#include "Property.hpp"

inline auto add_node(OSSIA::Node& n, const std::string& name)
{
    return *n.emplace(n.children().end(), name);
}


class ModelMetadata;

namespace Scenario
{
class ProcessModel;
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;
}
namespace Ossia
{
namespace LocalTree
{
class Constraint;
class ISCORE_PLUGIN_OSSIA_EXPORT DocumentPlugin :
        public iscore::DocumentPlugin
{
    public:
        DocumentPlugin(
                std::shared_ptr<OSSIA::Device> localDev,
                iscore::Document& doc,
                QObject* parent);

        ~DocumentPlugin();

    private:
        void create();
        void cleanup();

        Constraint* m_root{};
        std::shared_ptr<OSSIA::Device> m_localDevice;
};

}
}
