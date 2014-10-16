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

void HelloWorldSettingsView::setPresenter(iscore::SettingsGroupPresenter* presenter)
{
	m_presenter = static_cast<HelloWorldSettingsPresenter*>(presenter);
}

void HelloWorldSettingsView::setText(QString text)
{
	m_lineEdit->setText(text);
}

void HelloWorldSettingsView::on_textChanged()
{
	auto newText = m_lineEdit->text();
	iscore::Command* cmd = new TextChangedCommand{m_previousText,
						   newText,
						   static_cast<iscore::SettingsGroupPresenter*>(m_presenter)};
	emit submitCommand(cmd);
	m_previousText = newText;
}
