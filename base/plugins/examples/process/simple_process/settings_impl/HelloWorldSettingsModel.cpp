#include "HelloWorldSettingsModel.hpp"
using namespace iscore;

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
