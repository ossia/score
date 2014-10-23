#pragma once
#include <interface/customcommand/CustomCommand.hpp>
#include <QAction>

class ScenarioCommand : public iscore::CustomCommand
{
		Q_OBJECT
	public:
		ScenarioCommand();
		virtual void populateMenus(iscore::MenubarManager*) override;
		virtual void populateToolbars() override;
		virtual void setPresenter(iscore::Presenter*) override;

	signals:
		void incrementProcesses();
		void decrementProcesses();

	private slots:
		void on_actionTrigger();

	private:
		QAction* m_action_Scenarioigate;
		iscore::Presenter* m_presenter{};
};
