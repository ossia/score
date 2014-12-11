#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>
#include <interface/inspector/InspectorWidgetFactoryInterface.hpp>


class InspectorControl : public iscore::PluginControlInterface
{
	public:
		InspectorControl():
			iscore::PluginControlInterface{"InspectorControl", nullptr}
		{

		}

		virtual void populateMenus(iscore::MenubarManager*) override
		{
		}

		virtual void populateToolbars() override
		{
		}

		virtual void setPresenter(iscore::Presenter*) override
		{
		}

	public slots:
		void on_newInspectorWidgetFactory(iscore::FactoryInterface *e)
		{
			m_factories.push_back(
						static_cast<InspectorWidgetFactoryInterface*>(e));
		}

	private:
		QVector<InspectorWidgetFactoryInterface*> m_factories;
};
