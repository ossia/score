#pragma once
#include <core/model/Model.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/View.hpp>

#include <core/plugin/PluginManager.hpp>
#include <core/settings/Settings.hpp>

#include <memory>
#include <QApplication>

namespace iscore
{
	class Application : public QObject
	{
			Q_OBJECT
		public:
			Application(int argc, char** argv);
			int exec() { return m_app->exec(); }

		public slots:
			void dispatchPlugin(QObject*);

		private:
			std::unique_ptr<QApplication> m_app;
			PluginManager m_pluginManager;
			Settings m_settings;

			// Are they polymorphic ? Don't think so... They just hold plug-ins.
			std::unique_ptr<Model> m_model;
			std::unique_ptr<View> m_view;
			std::unique_ptr<Presenter> m_presenter;
	};
}
