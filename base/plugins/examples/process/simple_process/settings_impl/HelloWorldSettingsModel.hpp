#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <QString>
#include <QObject>

class HelloWorldSettingsModel :
		public QObject,
		public iscore::SettingsGroupModel
{
		Q_OBJECT
	public:
		void setText(QString txt);
		QString getText() const;

		virtual void setPresenter(iscore::SettingsGroupPresenter* presenter) override;

	signals:
		void textChanged();

	private:
		QString helloText;


};


