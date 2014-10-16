#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>

namespace iscore
{
	class SettingsModel
	{
		public:
			SettingsModel()
			{

			}

			void addSettingsModel(std::unique_ptr<SettingsGroupModel>&& model)
			{
				m_pluginModels.insert(std::move(model));
			}

		private:
			std::set<std::unique_ptr<SettingsGroupModel>> m_pluginModels;
	};
}
