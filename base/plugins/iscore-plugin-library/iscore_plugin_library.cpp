#include "iscore_plugin_library.hpp"

#include <Library/Panel/LibraryPanelFactory.hpp>

iscore_plugin_library::iscore_plugin_library() :
    QObject {},
    iscore::PanelFactory_QtInterface {}
{
}

std::vector<iscore::PanelFactory*> iscore_plugin_library::panels()
{
    return {new Library::LibraryPanelFactory};
}
