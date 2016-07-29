#pragma once
#include <ossia/network/local/local.hpp>
#include <ossia/network/generic/generic_device.hpp>
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
                iscore::Document& doc,
                QObject* parent);

        ~DocumentPlugin();

        impl::BasicDevice& device() { return m_localDevice; }
        const impl::BasicDevice& device() const { return m_localDevice; }

    private:
        void create();
        void cleanup();

        Constraint* m_root{};
        impl::BasicDevice m_localDevice;
};

}
}
