#pragma once
#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>


#include <vector>
#include <memory>
#include <QApplication>

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
			Presenter* presenter() { return m_presenter; }
			Settings* settings() { return m_settings.get(); }

		protected:
			virtual void childEvent(QChildEvent*) override;

		private:
			void loadPerInstancePluginData();
			void loadGlobalPluginData();
			void doConnections();

			// Base stuff.
			std::unique_ptr<QApplication> m_app;
			std::unique_ptr<Settings> m_settings; // Global settings

			// MVP
			Model* m_model{};
			View* m_view{};
			Presenter* m_presenter{};

			// Data
			PluginManager m_pluginManager;
	};
}
