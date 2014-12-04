#include "ScenarioSettingsModel.hpp"
#include <QDebug>
using namespace iscore;

ScenarioSettingsModel::ScenarioSettingsModel():
	iscore::SettingsDelegateModelInterface{}
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


void ScenarioSettingsModel::setPresenter(SettingsDelegatePresenterInterface* presenter)
{
}
