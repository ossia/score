#include "ApplicationContext.hpp"


iscore::ApplicationContext::ApplicationContext(
        const iscore::ApplicationSettings& app,
        const iscore::ApplicationComponents& c,
        iscore::DocumentManager& d,
        iscore::MenubarManager& m,
        const std::vector<iscore::SettingsDelegateModelInterface*>& set):
    applicationSettings{app},
    components{c},
    documents{d},
    menuBar{m},
    m_settings{set}
{

}
