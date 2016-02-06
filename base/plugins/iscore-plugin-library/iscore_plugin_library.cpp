#include "iscore_plugin_library.hpp"

#include <Library/Panel/LibraryPanelFactory.hpp>

iscore_plugin_library::iscore_plugin_library() :
    QObject {},
    iscore::PanelFactory_QtInterface {}
{
}

iscore_plugin_library::~iscore_plugin_library()
{

}

std::vector<iscore::PanelFactory*> iscore_plugin_library::panels()
{
    return {new Library::LibraryPanelFactory};
}

iscore::Version iscore_plugin_library::version() const
{
    return iscore::Version{1};
}

UuidKey<iscore::Plugin> iscore_plugin_library::key() const
{
    return "f019a413-0ffd-417f-966a-a824548aca79";
}
