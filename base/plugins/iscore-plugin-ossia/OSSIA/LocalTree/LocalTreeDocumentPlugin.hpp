#pragma once
#include <ossia/network/base/node.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
#include <iscore/tools/Metadata.hpp>


#include "SetProperty.hpp"
#include "GetProperty.hpp"
#include "Property.hpp"

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
                OSSIA::net::Device& localDev,
                iscore::Document& doc,
                QObject* parent);

        ~DocumentPlugin();

    private:
        void create();
        void cleanup();

        Constraint* m_root{};
        OSSIA::net::Device& m_localDevice;
};

}
}
