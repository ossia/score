// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationContext.hpp"

score::ApplicationContext::ApplicationContext(
    const score::ApplicationSettings& app,
    const score::ApplicationComponents& c,
    DocumentList& l,
    const std::vector<std::unique_ptr<score::SettingsDelegateModel>>& set)
    : applicationSettings{app}, components{c}, documents{l}, m_settings{set}
{
}

score::ApplicationContext::~ApplicationContext() = default;
