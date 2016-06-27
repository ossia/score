#pragma once
#include <iscore_plugin_ossia_export.h>
namespace iscore
{
struct DocumentContext;
template<typename T, typename U, typename V>
class GenericComponentFactoryList;
}
namespace Explorer
{
class DeviceDocumentPlugin;
}
namespace Process
{
class ProcessModel;
class StateProcess;
}

namespace RecreateOnPlay
{
class DocumentPlugin;
class ProcessComponent;
class ProcessComponentFactory;
class StateProcessComponent;
class StateProcessComponentFactory;

using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponentFactory>;

using StateProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
        Process::StateProcess,
        RecreateOnPlay::DocumentPlugin,
        RecreateOnPlay::StateProcessComponentFactory>;

struct ISCORE_PLUGIN_OSSIA_EXPORT Context
{
    const iscore::DocumentContext& doc;
    const RecreateOnPlay::DocumentPlugin& sys;
    const Explorer::DeviceDocumentPlugin& devices;
    const RecreateOnPlay::ProcessComponentFactoryList& processes;
    const RecreateOnPlay::StateProcessComponentFactoryList& stateProcesses;
};
}
