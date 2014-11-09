#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsModel.hpp"
#include "PluginSettingsView.hpp"

#include <core/settings/SettingsPresenter.hpp>
#include "commands/BlacklistCommand.hpp"

/*#include "commands/ClientPortChangedCommand.hpp"
#include "commands/MasterPortChangedCommand.hpp"
#include "commands/ClientNameChangedCommand.hpp"
*/
using namespace iscore;

PluginSettingsPresenter::PluginSettingsPresenter(SettingsPresenter* parent,
												 SettingsDelegateModelInterface* model,
												 SettingsDelegateViewInterface* view):
	SettingsDelegatePresenterInterface{parent, model, view}
{
	auto ps_model = static_cast<PluginSettingsModel*>(model);
	auto ps_view  = static_cast<PluginSettingsView*>(view);

	ps_view->view()->setModel(ps_model->model());

	/*
	connect(net_model, SIGNAL(masterPortChanged()),
			this,	   SLOT(updateMasterPort()));
	connect(net_model, SIGNAL(clientPortChanged()),
			this,	   SLOT(updateClientPort()));
	connect(net_model, SIGNAL(clientNameChanged()),
			this,	   SLOT(updateClientName()));
			*/
}

void PluginSettingsPresenter::on_accept()
{
	if(m_blacklistCommand)
		m_blacklistCommand->redo();

	delete m_blacklistCommand; m_blacklistCommand = nullptr;
	/*
	if(m_masterportCommand)
		m_masterportCommand->redo();
	if(m_clientportCommand)
		m_clientportCommand->redo();
	if(m_clientnameCommand)
		m_clientnameCommand->redo();

	delete m_masterportCommand;	m_masterportCommand = nullptr;
	delete m_clientportCommand;	m_clientportCommand = nullptr;
	delete m_clientnameCommand;	m_clientnameCommand = nullptr;
*/}

void PluginSettingsPresenter::on_reject()
{

	delete m_blacklistCommand; m_blacklistCommand = nullptr;
	/*
	if(m_masterportCommand)
		m_masterportCommand->undo();
	if(m_clientportCommand)
		m_clientportCommand->undo();
	if(m_clientnameCommand)
		m_clientnameCommand->undo();

	delete m_masterportCommand;	m_masterportCommand = nullptr;
	delete m_clientportCommand;	m_clientportCommand = nullptr;
	delete m_clientnameCommand;	m_clientnameCommand = nullptr;
*/}

void PluginSettingsPresenter::load()
{
/*	updateMasterPort();
	updateClientPort();
	updateClientName();
*/
	view()->load();
}
/*
// Partie modèle -> vue
void PluginSettingsPresenter::updateMasterPort()
{
	view()->setMasterPort(model()->getMasterPort());
}
void PluginSettingsPresenter::updateClientPort()
{
	view()->setClientPort(model()->getClientPort());
}
void PluginSettingsPresenter::updateClientName()
{
	view()->setClientName(model()->getClientName());
}

// Partie vue -> commande
void PluginSettingsPresenter::setMasterPortCommand(MasterPortChangedCommand* cmd)
{
	if(!m_masterportCommand)
		m_masterportCommand = cmd;
	else
	{
		m_masterportCommand->mergeWith(cmd);
		delete cmd;
	}
}
void PluginSettingsPresenter::setClientPortCommand(ClientPortChangedCommand* cmd)
{
	if(!m_clientportCommand)
		m_clientportCommand = cmd;
	else
	{
		m_clientportCommand->mergeWith(cmd);
		delete cmd;
	}
}
void PluginSettingsPresenter::setClientNameCommand(ClientNameChangedCommand* cmd)
{
	if(!m_clientnameCommand)
		m_clientnameCommand = cmd;
	else
	{
		m_clientnameCommand->mergeWith(cmd);
		delete cmd;
	}
}
*/

PluginSettingsModel*PluginSettingsPresenter::model()
{
	return static_cast<PluginSettingsModel*>(m_model);
}

PluginSettingsView*PluginSettingsPresenter::view()
{
	return static_cast<PluginSettingsView*>(m_view);
}

void PluginSettingsPresenter::setBlacklistCommand(BlacklistCommand* cmd)
{
	if(!m_blacklistCommand)
		m_blacklistCommand = cmd;
	else
	{
		m_blacklistCommand->mergeWith(cmd);
		delete cmd;
	}
}

#include <QApplication>
#include <QStyle>
QIcon PluginSettingsPresenter::settingsIcon()
{
	return QApplication::style()->standardIcon(QStyle::SP_CommandLink);
}
