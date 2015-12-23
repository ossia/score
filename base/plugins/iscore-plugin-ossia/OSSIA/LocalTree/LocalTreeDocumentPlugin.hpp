#pragma once
#include <Network/Node.h>
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
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;
namespace Scenario
{
class ScenarioModel;

}
namespace OSSIA
{
namespace LocalTree
{

struct ScenarioVisitor
{
        void visit(Scenario::ScenarioModel& scenario,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(ModelMetadata& metadata,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(ConstraintModel& constraint,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(EventModel& ev,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(TimeNodeModel& tn,
                   const std::shared_ptr<OSSIA::Node>& parent);

        void visit(StateModel& state,
                   const std::shared_ptr<OSSIA::Node>& parent);
};

struct TreeComponent {
        ISCORE_METADATA(OSSIA::LocalTree::TreeComponent)
};
class DocumentPlugin : public iscore::DocumentPluginModel
{
        std::shared_ptr<OSSIA::Device> m_localDevice;
        ISCORE_METADATA(OSSIA::LocalTree::DocumentPlugin)
    public:

        DocumentPlugin(
                std::shared_ptr<OSSIA::Device> localDev,
                iscore::Document& doc,
                QObject* parent):
            iscore::DocumentPluginModel{doc, "LocalTreeDocumentPlugin", parent},
            m_localDevice{localDev}
        {

        }
};
}
}
