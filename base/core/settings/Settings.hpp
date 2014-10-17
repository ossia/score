#pragma once
#include <memory>
#include <interface/settings/SettingsGroup.hpp>
#include <set>

#include <core/settings/SettingsModel.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>

namespace iscore
{
	// Les settings ont leur propre "file de commande" pour ne pas interférer avec le reste.
	// (on ne "undo" généralement pas les settings).
	class Settings : public QObject
	{
		public:
			Settings(QObject* parent);

			void addSettingsPlugin(SettingsGroup* plugin);
			SettingsView*  view()  { return m_settingsView;  }
			SettingsModel* model() { return m_settingsModel; }

		private:
			SettingsModel* m_settingsModel;
			SettingsView* m_settingsView;
			SettingsPresenter* m_settingsPresenter;
	};
}
