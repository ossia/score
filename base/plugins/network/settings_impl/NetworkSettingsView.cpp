#include "NetworkSettingsView.hpp"
#include <QHBoxLayout>
#include "NetworkSettingsPresenter.hpp"
#include "TextChangedCommand.hpp"

using namespace iscore;

NetworkSettingsView::NetworkSettingsView(QWidget* parent):
	QWidget{parent},
	iscore::SettingsGroupView{},
	m_lineEdit{new QLineEdit(this)}
{
	auto layout = new QHBoxLayout(this);
	this->setLayout(layout);

	layout->addWidget(m_lineEdit);

	connect(m_lineEdit, &QLineEdit::textChanged,
			this,		&NetworkSettingsView::on_textChanged);
}

void NetworkSettingsView::setPresenter(iscore::SettingsGroupPresenter* presenter)
{
	m_presenter = static_cast<NetworkSettingsPresenter*>(presenter);
}

void NetworkSettingsView::setText(QString text)
{
	if(text != m_lineEdit->text())
		m_lineEdit->setText(text);
}

QWidget* NetworkSettingsView::getWidget()
{
	return static_cast<QWidget*>(this);
}

void NetworkSettingsView::on_textChanged()
{
	auto newText = m_lineEdit->text();
	if(newText != m_previousText)
	{
		iscore::Command* cmd = new TextChangedCommand{m_previousText,
							   newText,
							   static_cast<iscore::SettingsGroupPresenter*>(m_presenter)};
		emit submitCommand(cmd);
		m_previousText = newText;
	}
}
