#include "NetworkSettingsPresenter.hpp"
#include "NetworkSettingsModel.hpp"
#include "NetworkSettingsView.hpp"

#include <core/settings/SettingsPresenter.hpp>

#include "commands/ClientPortChangedCommand.hpp"
#include "commands/MasterPortChangedCommand.hpp"
#include "commands/ClientNameChangedCommand.hpp"

using namespace iscore;

NetworkSettingsPresenter::NetworkSettingsPresenter(SettingsPresenter* parent,
        SettingsDelegateModelInterface* model,
        SettingsDelegateViewInterface* view) :
    SettingsDelegatePresenterInterface {parent, model, view}
{
    auto net_model = static_cast<NetworkSettingsModel*>(model);
    connect(net_model, SIGNAL(masterPortChanged()),
    this,	   SLOT(updateMasterPort()));
    connect(net_model, SIGNAL(clientPortChanged()),
    this,	   SLOT(updateClientPort()));
    connect(net_model, SIGNAL(clientNameChanged()),
    this,	   SLOT(updateClientName()));
}

void NetworkSettingsPresenter::on_accept()
{
    if(m_masterportCommand)
    {
        m_masterportCommand->redo();
    }

    if(m_clientportCommand)
    {
        m_clientportCommand->redo();
    }

    if(m_clientnameCommand)
    {
        m_clientnameCommand->redo();
    }

    delete m_masterportCommand;
    m_masterportCommand = nullptr;
    delete m_clientportCommand;
    m_clientportCommand = nullptr;
    delete m_clientnameCommand;
    m_clientnameCommand = nullptr;
}

void NetworkSettingsPresenter::on_reject()
{
    if(m_masterportCommand)
    {
        m_masterportCommand->undo();
    }

    if(m_clientportCommand)
    {
        m_clientportCommand->undo();
    }

    if(m_clientnameCommand)
    {
        m_clientnameCommand->undo();
    }

    delete m_masterportCommand;
    m_masterportCommand = nullptr;
    delete m_clientportCommand;
    m_clientportCommand = nullptr;
    delete m_clientnameCommand;
    m_clientnameCommand = nullptr;
}

void NetworkSettingsPresenter::load()
{
    updateMasterPort();
    updateClientPort();
    updateClientName();

    view()->load();
}

// Partie modèle -> vue
void NetworkSettingsPresenter::updateMasterPort()
{
    view()->setMasterPort(model()->getMasterPort());
}
void NetworkSettingsPresenter::updateClientPort()
{
    view()->setClientPort(model()->getClientPort());
}
void NetworkSettingsPresenter::updateClientName()
{
    view()->setClientName(model()->getClientName());
}

// Partie vue -> commande
void NetworkSettingsPresenter::setMasterPortCommand(MasterPortChangedCommand* cmd)
{
    ISCORE_TODO
    /*
    if(!m_masterportCommand)
    {
        m_masterportCommand = cmd;
    }
    else
    {
        m_masterportCommand->mergeWith(cmd);
        delete cmd;
    }
    */
}
void NetworkSettingsPresenter::setClientPortCommand(ClientPortChangedCommand* cmd)
{
    ISCORE_TODO
    /*
    if(!m_clientportCommand)
    {
        m_clientportCommand = cmd;
    }
    else
    {
        m_clientportCommand->mergeWith(cmd);
        delete cmd;
    }
    */
}
void NetworkSettingsPresenter::setClientNameCommand(ClientNameChangedCommand* cmd)
{
    ISCORE_TODO
    /*
    if(!m_clientnameCommand)
    {
        m_clientnameCommand = cmd;
    }
    else
    {
        m_clientnameCommand->mergeWith(cmd);
        delete cmd;
    }
    */
}


NetworkSettingsModel* NetworkSettingsPresenter::model()
{
    return static_cast<NetworkSettingsModel*>(m_model);
}

NetworkSettingsView* NetworkSettingsPresenter::view()
{
    return static_cast<NetworkSettingsView*>(m_view);
}

#include <QApplication>
#include <QStyle>
QIcon NetworkSettingsPresenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_DriveNetIcon);
}
