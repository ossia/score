#pragma once
#include <interface/customcommand/CustomCommand.hpp>
#include <QAction>
#include <QPointF>

class ScenarioCommand : public iscore::CustomCommand
{
		Q_OBJECT
	public:
		ScenarioCommand();
		virtual void populateMenus(iscore::MenubarManager*) override;
		virtual void populateToolbars() override;
		virtual void setPresenter(iscore::Presenter*) override;
        void emitCreateTimeEvent(QPointF pos);

	signals:
		void incrementProcesses();
		void decrementProcesses();
        void createTimeEvent(QPointF pos);

	private slots:
        void on_createTimeEvent(QPointF position);

	private:
        iscore::Presenter* m_presenter{};
};
