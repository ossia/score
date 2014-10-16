#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/Command.hpp>
#include <QObject>

class HelloWorldSettingsPresenter : public QObject, public iscore::SettingsGroupPresenter
{
		Q_OBJECT
	public:
		HelloWorldSettingsPresenter(iscore::SettingsGroupModel* model,
									iscore::SettingsGroupView* view);

		void setText(QString text);

	public slots:
		void updateViewText();
		void submitCommand(iscore::Command* cmd);


};
