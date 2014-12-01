#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>

class ScenarioControl : public iscore::PluginControlInterface
{
	public:
		ScenarioControl(QObject* parent);

		virtual void populateMenus(iscore::MenubarManager*) override;
		virtual void populateToolbars() override;
		virtual void setPresenter(iscore::Presenter*) override;

		virtual iscore::SerializableCommand* instantiateUndoCommand(QString name, QByteArray data) override;


};
