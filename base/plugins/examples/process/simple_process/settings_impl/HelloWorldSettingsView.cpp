#include "HelloWorldSettingsView.hpp"
#include <QHBoxLayout>
#include "HelloWorldSettingsPresenter.hpp"
#include "TextChangedCommand.hpp"

using namespace iscore;

HelloWorldSettingsView::HelloWorldSettingsView(QWidget* parent):
	QWidget{parent},
	iscore::SettingsGroupView{},
	m_lineEdit{new QLineEdit(this)}
{
	auto layout = new QHBoxLayout(this);
	this->setLayout(layout);

	layout->addWidget(m_lineEdit);

	connect(m_lineEdit, &QLineEdit::textChanged,
			this,		&HelloWorldSettingsView::on_textChanged);
}

void HelloWorldSettingsView::setText(QString text)
{
	if(text != m_lineEdit->text())
		m_lineEdit->setText(text);
}

QWidget* HelloWorldSettingsView::getWidget()
{
	return static_cast<QWidget*>(this);
}

void HelloWorldSettingsView::on_textChanged()
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
