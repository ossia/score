#include "ScenarioSettingsModel.hpp"
#include <QDebug>
using namespace iscore;

ScenarioSettingsModel::ScenarioSettingsModel():
	iscore::SettingsGroupModel{}
{
	this->setObjectName("ScenarioSettingsModel");
}

void ScenarioSettingsModel::setText(QString txt)
{
	helloText = txt;
	emit textChanged();
}

QString ScenarioSettingsModel::getText() const
{
	return helloText;
}


void ScenarioSettingsModel::setPresenter(SettingsGroupPresenter* presenter)
{
}
