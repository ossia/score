#include "ApplicationContext.hpp"

iscore::ApplicationContext::ApplicationContext(
    const iscore::ApplicationSettings& app,
    const iscore::ApplicationComponents& c,
    DocumentList& l,
    const std::vector<std::unique_ptr<iscore::SettingsDelegateModel>>& set)
    : applicationSettings{app}
    , components{c}
    , documents{l}
    , m_settings{set}
{
}

iscore::ApplicationContext::~ApplicationContext() = default;
