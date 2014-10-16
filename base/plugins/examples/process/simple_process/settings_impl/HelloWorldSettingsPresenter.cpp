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
	auto hw_view = static_cast<HelloWorldSettingsView*>(view);
	auto hw_model = static_cast<HelloWorldSettingsModel*>(model);
	connect(hw_view, SIGNAL(submitCommand(iscore::Command*)),
			this, SLOT(submitCommand(iscore::Command*)));

	connect(hw_model, SIGNAL(textChanged()),
			this, SLOT(updateViewText()));
}

void HelloWorldSettingsPresenter::setText(QString text)
{
	auto model = dynamic_cast<HelloWorldSettingsModel*>(m_model);
	model->setText(text); // Ou setter.
}

void HelloWorldSettingsPresenter::updateViewText()
{
	auto model = dynamic_cast<HelloWorldSettingsModel*>(m_model);
	auto view = dynamic_cast<HelloWorldSettingsView*>(m_view);
	view->setText(model->getText());
}

void HelloWorldSettingsPresenter::submitCommand(iscore::Command* cmd)
{
	m_parentPresenter->submitCommand(cmd);
}
