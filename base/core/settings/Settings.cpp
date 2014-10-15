#include <core/settings/Settings.hpp>

using namespace iscore;

Settings::Settings()
{
}

void Settings::addSettingsPlugin(std::unique_ptr<iscore::SettingsGroup>&& plugin)
{
	m_plugins.insert(std::move(plugin));
}
