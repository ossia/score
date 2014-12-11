#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>
#include <core/processes/ProcessList.hpp>

class ScenarioControl : public iscore::PluginControlInterface
{
	public:
		ScenarioControl(QObject* parent);

		virtual void populateMenus(iscore::MenubarManager*) override;
		virtual void populateToolbars() override;
		virtual void setPresenter(iscore::Presenter*) override;

		virtual iscore::SerializableCommand* instantiateUndoCommand(QString name, QByteArray data) override;

		ProcessList* processList()
		{
			return m_processList;
		}

	private:
		ProcessList* m_processList{};


};
