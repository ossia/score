#include "PluginSettingsView.hpp"
#include <QGridLayout>
#include <QLabel>
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsModel.hpp"
/*#include "commands/ClientPortChangedCommand.hpp"
#include "commands/MasterPortChangedCommand.hpp"
#include "commands/ClientNameChangedCommand.hpp"
*/
#include <QApplication>
using namespace iscore;

PluginSettingsView::PluginSettingsView(QWidget* parent):
	QWidget{parent},
	iscore::SettingsGroupView{}
{
	auto layout = new QGridLayout(this);
	this->setLayout(layout);

	m_listView = new QListView{this};
/*
	m_masterPort->setMinimum(1001);
	m_clientPort->setMinimum(1001);
	m_masterPort->setMaximum(65535);
	m_clientPort->setMaximum(65535);

	layout->addWidget(new QLabel{"Master port"}, 0, 0);
	layout->addWidget(m_masterPort, 0, 1);

	layout->addWidget(new QLabel{"Client port"}, 1, 0);
	layout->addWidget(m_clientPort, 1, 1);

	layout->addWidget(new QLabel{"Client Name"}, 2, 0);
	layout->addWidget(m_clientName, 2, 1);*/
}
/*
void PluginSettingsView::setClientName(QString text)
{
	if(text != m_clientName->text())
		m_clientName->setText(text);
}
void PluginSettingsView::setMasterPort(int val)
{
	if(val != m_masterPort->value())
		m_masterPort->setValue(val);
}
void PluginSettingsView::setClientPort(int val)
{
	if(val != m_clientPort->value())
		m_clientPort->setValue(val);
}
*/
QWidget* PluginSettingsView::getWidget()
{
	return static_cast<QWidget*>(this);
}

void PluginSettingsView::load()
{/*
	m_previousMasterPort = m_masterPort->value();
	m_previousClientPort = m_clientPort->value();
	m_previousClientName = m_clientName->text();
*/}

void PluginSettingsView::doConnections()
{/*
	connect(m_clientName,	&QLineEdit::textChanged,
			this,			&PluginSettingsView::on_clientNameChanged);

	// http://stackoverflow.com/questions/16794695/qt5-overloaded-signals-and-slots
	connect(m_masterPort,	SIGNAL(valueChanged(int)),
			this,			SLOT(on_masterPortChanged(int)));
	connect(m_clientPort,	SIGNAL(valueChanged(int)),
			this,			SLOT(on_clientPortChanged(int)));
*/}
/*
void PluginSettingsView::on_masterPortChanged(int x)
{
	auto newVal = m_masterPort->value();
	if(newVal != m_previousMasterPort)
	{
		presenter()->setMasterPortCommand(new MasterPortChangedCommand{m_previousMasterPort, newVal});
		m_previousMasterPort = newVal;
	}
}

void PluginSettingsView::on_clientPortChanged(int)
{
	auto newVal = m_clientPort->value();
	if(newVal != m_previousClientPort)
	{
		presenter()->setClientPortCommand(new ClientPortChangedCommand{m_previousClientPort, newVal});
		m_previousClientPort = newVal;
	}
}
void PluginSettingsView::on_clientNameChanged()
{
	auto newText = m_clientName->text();
	if(newText != m_previousClientName)
	{
		presenter()->setClientNameCommand(new ClientNameChangedCommand{m_previousClientName, newText});
		m_previousClientName = newText;
	}
}
*/
PluginSettingsPresenter* PluginSettingsView::presenter()
{
	return static_cast<PluginSettingsPresenter*>(m_presenter);
}
