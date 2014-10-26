#include "NetworkSettingsPresenter.hpp"
#include "NetworkSettingsModel.hpp"
#include "NetworkSettingsView.hpp"

#include <core/settings/SettingsPresenter.hpp>
using namespace iscore;

NetworkSettingsPresenter::NetworkSettingsPresenter(SettingsPresenter* parent,
														 SettingsGroupModel* model,
														 SettingsGroupView* view):
	SettingsGroupPresenter{parent, model, view}
{
	auto hw_view = static_cast<NetworkSettingsView*>(view);
	auto hw_model = static_cast<NetworkSettingsModel*>(model);
	connect(hw_view, SIGNAL(submitCommand(iscore::Command*)),
			this, SLOT(on_submitCommand(iscore::Command*)));

	connect(hw_model, SIGNAL(textChanged()),
			this, SLOT(updateViewText()));
}

void NetworkSettingsPresenter::setText(QString text)
{
	auto model = dynamic_cast<NetworkSettingsModel*>(m_model);
	model->setText(text); // Ou setter.
}

void NetworkSettingsPresenter::on_accept()
{
	if(m_currentCommand)
		m_currentCommand->redo();
}

void NetworkSettingsPresenter::on_reject()
{
	delete m_currentCommand;
	m_currentCommand = nullptr;
}

void NetworkSettingsPresenter::updateViewText()
{
	auto model = dynamic_cast<NetworkSettingsModel*>(m_model);
	auto view = dynamic_cast<NetworkSettingsView*>(m_view);
	view->setText(model->getText());
}

void NetworkSettingsPresenter::on_submitCommand(iscore::Command* cmd)
{
	delete m_currentCommand;
	m_currentCommand = cmd;
}
