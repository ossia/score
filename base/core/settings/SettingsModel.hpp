#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>
#include <interface/settings/SettingsGroupModel.hpp>
#include <QObject>

namespace iscore
{
	class SettingsModel : public QObject
	{
		public:
			using QObject::QObject;

			void addSettingsModel(std::unique_ptr<SettingsGroupModel>&& model)
			{
				model->setParent(this); // TODO careful with double-deletion.
				m_pluginModels.insert(std::move(model));
			}

		private:
			std::set<std::unique_ptr<SettingsGroupModel>> m_pluginModels;
	};
}
