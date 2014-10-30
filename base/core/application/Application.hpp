#pragma once
#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>
#include <QNamedObject>

#include <vector>
#include <memory>
#include <QApplication>

namespace iscore
{
	class Model;
	class Presenter;
	class View;

	/**
	 * @brief Application
	 * 
	 * This class is the main object in i-score. It is the 
	 * parent of every other object created.
	 * It does instantiate the rest of the software (MVP, settings, plugins).
	 */
	class Application : public QNamedObject
	{
			Q_OBJECT
		public:
			Application(int& argc, char** argv);
			~Application();

			int exec() { return m_app->exec(); }
			View* view() { return m_view; }
			Presenter* presenter() { return m_presenter; }
			Settings* settings() { return m_settings.get(); }

			void doConnections();
			void doConnections(QObject*);

		public slots:
			void addAutoconnection(Autoconnect);

		private:
			void loadPluginData();

			// Base stuff.
			std::unique_ptr<QApplication> m_app;
			std::unique_ptr<Settings> m_settings; // Global settings

			// MVP
			Model* m_model{};
			View* m_view{};
			Presenter* m_presenter{};

			// Data
			PluginManager m_pluginManager{this};
	};
}
