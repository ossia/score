#pragma once
#include <core/processes/ProcessList.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>

#include <interface/autoconnect/Autoconnect.hpp>

#include <vector>
#include <memory>
#include <QApplication>

#include <API/Headers/Repartition/session/Session.h>

namespace iscore
{
	class Model;
	class Presenter;
	class View;

	class Application : public QObject
	{
			Q_OBJECT
		public:
	                Application(int& argc, char** argv);
			~Application();

			int exec() { return m_app->exec(); }
			View* view() { return m_view; }
			Settings* settings() { return m_settings.get(); }

			void setupMasterSession();
			void setupClientSession();

		public slots:
			// Cela mérite-t-il d'avoir un objet propre ?
			void dispatchPlugin(QObject*);

		protected:
			virtual void childEvent(QChildEvent*) override;

		private:
			void doConnections();

			// Base stuff.
			std::unique_ptr<QApplication> m_app;
			std::unique_ptr<Settings> m_settings; // Global settings
			std::unique_ptr<Session> m_networkSession; // For distribution

			// MVP
			Model* m_model{};
			View* m_view{};
			Presenter* m_presenter{};

			// Data
			std::vector<Autoconnect> m_autoconnections; // TODO try unordered_set
			ProcessList m_processList;
			PluginManager m_pluginManager;
	};
}
