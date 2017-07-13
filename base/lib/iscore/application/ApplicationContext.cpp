// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
