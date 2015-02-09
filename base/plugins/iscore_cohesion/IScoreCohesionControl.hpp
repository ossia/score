#pragma once
#include <interface/plugincontrol/PluginControlInterface.hpp>

class IScoreCohesionControl : public iscore::PluginControlInterface
{
	public:
		IScoreCohesionControl(QObject* parent);
		void populateMenus(iscore::MenubarManager *) override;
		void populateToolbars() override { }
		void setPresenter(iscore::Presenter *) override { }
		iscore::SerializableCommand* instantiateUndoCommand(QString name, QByteArray data);

	public slots:
		void createCurvesFromAddresses();
};
