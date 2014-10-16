#include "HelloWorldSettingsModel.hpp"
#include <QDebug>
using namespace iscore;

HelloWorldSettingsModel::HelloWorldSettingsModel():
	iscore::SettingsGroupModel{}
{
	this->setObjectName("HelloWorldSettingsModel");
}

void HelloWorldSettingsModel::setText(QString txt)
{
	helloText = txt;
	emit textChanged();
}

QString HelloWorldSettingsModel::getText() const
{
	return helloText;
}


void HelloWorldSettingsModel::setPresenter(SettingsGroupPresenter* presenter)
{
}
