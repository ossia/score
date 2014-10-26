#include "NetworkSettingsModel.hpp"
#include <QDebug>
using namespace iscore;

NetworkSettingsModel::NetworkSettingsModel():
	iscore::SettingsGroupModel{}
{
	this->setObjectName("NetworkSettingsModel");
}

void NetworkSettingsModel::setText(QString txt)
{
	helloText = txt;
	emit textChanged();
}

QString NetworkSettingsModel::getText() const
{
	return helloText;
}


void NetworkSettingsModel::setPresenter(SettingsGroupPresenter* presenter)
{
}
