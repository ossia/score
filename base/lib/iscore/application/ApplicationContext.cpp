#include "ApplicationContext.hpp"


iscore::ApplicationContext::ApplicationContext(
        const iscore::ApplicationSettings& app,
        const iscore::ApplicationComponents& c,
        iscore::DocumentManager& d,
        iscore::MenuManager& m,
        iscore::ToolbarManager& t,
        iscore::ActionManager& a,
        const std::vector<iscore::SettingsDelegateModelInterface*>& set):
    applicationSettings{app},
    components{c},
    documents{d},
    menus{m},
    toolbars{t},
    actions{a},
    m_settings{set}
{

}

iscore::ApplicationContext::~ApplicationContext() = default;
