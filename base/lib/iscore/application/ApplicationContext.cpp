#include "ApplicationContext.hpp"


iscore::ApplicationContext::ApplicationContext(
        const iscore::ApplicationSettings& app,
        const iscore::Settings& set,
        const iscore::ApplicationComponents& c,
        iscore::DocumentManager& d,
        iscore::MenubarManager& m):
    applicationSettings{app},
    settings{set},
    components{c},
    documents{d},
    menuBar{m}
{

}
