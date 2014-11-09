#include "ScenarioSettingsView.hpp"
#include <QHBoxLayout>
#include "ScenarioSettingsPresenter.hpp"
#include "TextChangedCommand.hpp"

using namespace iscore;

ScenarioSettingsView::ScenarioSettingsView(QObject* parent):
	iscore::SettingsDelegateViewInterface{parent}
{
	auto layout = new QHBoxLayout(m_widget);
	m_widget->setLayout(layout);

	layout->addWidget(m_lineEdit);

	connect(m_lineEdit, &QLineEdit::textChanged,
			this,		&ScenarioSettingsView::on_textChanged);
}

void ScenarioSettingsView::setText(QString text)
{
	if(text != m_lineEdit->text())
		m_lineEdit->setText(text);
}

QWidget* ScenarioSettingsView::getWidget()
{
	return m_widget;
}

void ScenarioSettingsView::on_textChanged()
{
	auto newText = m_lineEdit->text();
	if(newText != m_previousText)
	{
		iscore::Command* cmd = new TextChangedCommand{m_previousText,
							   newText,
							   getPresenter()};
		emit submitCommand(cmd);
		m_previousText = newText;
	}
}
