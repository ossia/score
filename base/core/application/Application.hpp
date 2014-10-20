#pragma once
#include <core/model/Model.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>

#include <interface/autoconnect/Autoconnect.hpp>
#include <memory>
#include <QApplication>
#include <vector>
#include <interface/processes/ProcessModel.hpp>
namespace iscore
{
	class Application : public QObject
	{
			Q_OBJECT
		public:
			Application(int argc, char** argv);

			int exec() { return m_app->exec(); }
			View* view() { return m_view; }
			Settings* settings() { return m_settings.get(); }

			//FOR TESTING
			ProcessModel* pm;

		public slots:
			// Cela mérite-t-il d'avoir un objet propre ?
			void dispatchPlugin(QObject*);

		protected:
			virtual void childEvent(QChildEvent*) override;

		private:
			void doConnections();

			// Base stuff.
			std::unique_ptr<QApplication> m_app;
			PluginManager m_pluginManager;
			std::unique_ptr<Settings> m_settings; // Global settings

			Model* m_model{};
			View* m_view{};
			Presenter* m_presenter{};

			std::vector<Autoconnect> m_autoconnections; // try unordered_set


	};
}
