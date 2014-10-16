#pragma once
#include <set>
#include <memory>
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/Command.hpp>
#include <QDebug>

namespace iscore
{
	class SettingsModel;
	class SettingsView;
	class SettingsPresenter
	{
		public:
		SettingsPresenter(SettingsModel* model, SettingsView* view):
			m_model{model},
			m_view{view}
		{

		}

		void addSettingsPresenter(std::unique_ptr<SettingsGroupPresenter>&& presenter)
		{
			m_pluginPresenters.insert(std::move(presenter));
		}

		void submitCommand(Command* cmd)
		{
			qDebug() << "Command executed: " << cmd->text();
			cmd->redo();
		}

		private:
			SettingsModel* m_model;
			SettingsView* m_view;

			std::set<std::unique_ptr<SettingsGroupPresenter>> m_pluginPresenters;
	};
}
