#pragma once
#include <set>
#include <memory>
#include <queue>
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/Command.hpp>
#include <QDebug>

namespace iscore
{
	class SettingsModel;
	class SettingsView;
	class SettingsPresenter : public QObject
	{
			Q_OBJECT
		public:
			SettingsPresenter(SettingsModel* model, SettingsView* view, QObject* parent);

			void addSettingsPresenter(SettingsGroupPresenter* presenter);

		private slots:
			void on_accept();
			void on_reject();

		private:
			SettingsModel* m_model;
			SettingsView* m_view;

			std::set<SettingsGroupPresenter*> m_pluginPresenters;
	};
}
