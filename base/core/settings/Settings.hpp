#pragma once
#include <memory>
#include <interface/settings/SettingsGroup.hpp>
#include <set>

#include <core/settings/SettingsModel.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>

namespace iscore
{
	class Settings
	{
		public:
			Settings();

			void addSettingsPlugin(std::unique_ptr<SettingsGroup>&& plugin);

		private:
			std::unique_ptr<SettingsModel> m_settingsModel;
			std::unique_ptr<SettingsView> m_settingsView;
			std::unique_ptr<SettingsPresenter> m_settingsPresenter;

			std::set<std::unique_ptr<SettingsGroup>> m_plugins;
	};
}
