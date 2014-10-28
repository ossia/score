#include "HelloWorldSettingsPresenter.hpp"
#include "HelloWorldSettingsModel.hpp"
#include "HelloWorldSettingsView.hpp"

#include <core/settings/SettingsPresenter.hpp>
using namespace iscore;

HelloWorldSettingsPresenter::HelloWorldSettingsPresenter(SettingsPresenter* parent,
														 SettingsGroupModel* model,
														 SettingsGroupView* view):
	SettingsGroupPresenter{parent, model, view}
{
	this->setObjectName("Hello World Settings");
	auto hw_view = static_cast<HelloWorldSettingsView*>(view);
	auto hw_model = static_cast<HelloWorldSettingsModel*>(model);
	connect(hw_view, SIGNAL(submitCommand(iscore::Command*)),
			this, SLOT(on_submitCommand(iscore::Command*)));

	connect(hw_model, SIGNAL(textChanged()),
			this, SLOT(updateViewText()));
}

void HelloWorldSettingsPresenter::setText(QString text)
{
	auto model = dynamic_cast<HelloWorldSettingsModel*>(m_model);
	model->setText(text); // Ou setter.
}

void HelloWorldSettingsPresenter::on_accept()
{
	if(m_currentCommand)
		m_currentCommand->redo();
}

void HelloWorldSettingsPresenter::on_reject()
{
	delete m_currentCommand;
	m_currentCommand = nullptr;
}

void HelloWorldSettingsPresenter::updateViewText()
{
	auto model = dynamic_cast<HelloWorldSettingsModel*>(m_model);
	auto view = dynamic_cast<HelloWorldSettingsView*>(m_view);
	view->setText(model->getText());
}

void HelloWorldSettingsPresenter::on_submitCommand(iscore::Command* cmd)
{
	delete m_currentCommand;
	m_currentCommand = cmd;
}
