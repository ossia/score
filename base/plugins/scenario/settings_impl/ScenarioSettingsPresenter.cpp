#include "ScenarioSettingsPresenter.hpp"
#include "ScenarioSettingsModel.hpp"
#include "ScenarioSettingsView.hpp"

#include <core/settings/SettingsPresenter.hpp>
using namespace iscore;

ScenarioSettingsPresenter::ScenarioSettingsPresenter(SettingsPresenter* parent,
														 SettingsDelegateModelInterface* model,
														 SettingsDelegateViewInterface* view):
	SettingsDelegatePresenterInterface{parent, model, view}
{
	this->setObjectName("Scenario plugin");
	auto hw_view = static_cast<ScenarioSettingsView*>(view);
	auto hw_model = static_cast<ScenarioSettingsModel*>(model);
	connect(hw_view, SIGNAL(submitCommand(iscore::Command*)),
			this, SLOT(on_submitCommand(iscore::Command*)));

	connect(hw_model, SIGNAL(textChanged()),
			this, SLOT(updateViewText()));
}

void ScenarioSettingsPresenter::setText(QString text)
{
	auto model = dynamic_cast<ScenarioSettingsModel*>(m_model);
	model->setText(text); // Ou setter.
}

void ScenarioSettingsPresenter::on_accept()
{
	if(m_currentCommand)
		m_currentCommand->redo();
}

void ScenarioSettingsPresenter::on_reject()
{
	delete m_currentCommand;
	m_currentCommand = nullptr;
}

void ScenarioSettingsPresenter::updateViewText()
{
	auto model = dynamic_cast<ScenarioSettingsModel*>(m_model);
	auto view = dynamic_cast<ScenarioSettingsView*>(m_view);
	view->setText(model->getText());
}

void ScenarioSettingsPresenter::on_submitCommand(iscore::Command* cmd)
{
	delete m_currentCommand;
	m_currentCommand = cmd;
}

#include <QApplication>
#include <QStyle>
QIcon ScenarioSettingsPresenter::settingsIcon()
{
	return QApplication::style()->standardIcon(QStyle::SP_MediaPlay);
}
