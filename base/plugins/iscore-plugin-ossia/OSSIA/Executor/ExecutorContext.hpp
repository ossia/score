#pragma once
namespace iscore
{
struct DocumentContext;
template<typename T, typename U, typename V, typename W>
class GenericComponentFactoryList;
}
class DeviceList;

namespace Process
{
class ProcessModel;
}

namespace RecreateOnPlay
{
class DocumentPlugin;
class ProcessComponent;
class ProcessComponentFactory;

using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponent,
            RecreateOnPlay::ProcessComponentFactory>;

struct Context
{
    const iscore::DocumentContext& doc;
    const DocumentPlugin& sys;
    const DeviceList& devices;
    const ProcessComponentFactoryList& processes;
};
}
