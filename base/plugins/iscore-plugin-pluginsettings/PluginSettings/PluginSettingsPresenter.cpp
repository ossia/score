#include <QApplication>
#include <QDebug>
#include <QListView>
#include <QStandardItemModel>
#include <QStyle>

#include "PluginSettingsModel.hpp"
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <iscore/command/Command.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenterInterface.hpp>
#include "PluginSettings/commands/BlacklistCommand.hpp"

namespace iscore {
class SettingsDelegateModelInterface;
class SettingsDelegateViewInterface;
class SettingsPresenter;
}  // namespace iscore

using namespace iscore;

namespace PluginSettings
{
PluginSettingsPresenter::PluginSettingsPresenter(SettingsPresenter* parent,
        SettingsDelegateModelInterface* model,
        SettingsDelegateViewInterface* view) :
    SettingsDelegatePresenterInterface {parent, model, view}
{
    auto ps_model = static_cast<PluginSettingsModel*>(model);
    auto ps_view  = static_cast<PluginSettingsView*>(view);

    ps_view->view()->setModel(ps_model->model());

    connect(ps_model,	&PluginSettingsModel::blacklistCommand,
    this,		&PluginSettingsPresenter::setBlacklistCommand);
}

void PluginSettingsPresenter::on_accept()
{
    if(m_blacklistCommand)
    {
        m_blacklistCommand->redo();
    }

    delete m_blacklistCommand;
    m_blacklistCommand = nullptr;
}

void PluginSettingsPresenter::on_reject()
{
    delete m_blacklistCommand;
    m_blacklistCommand = nullptr;
}

void PluginSettingsPresenter::load()
{
    view()->load();
}

PluginSettingsModel* PluginSettingsPresenter::model()
{
    return static_cast<PluginSettingsModel*>(m_model);
}

PluginSettingsView* PluginSettingsPresenter::view()
{
    return static_cast<PluginSettingsView*>(m_view);
}


void PluginSettingsPresenter::setBlacklistCommand(BlacklistCommand* cmd)
{
    ISCORE_TODO;
    /*
    if(!m_blacklistCommand)
    {
        m_blacklistCommand = cmd;
    }
    else
    {
        m_blacklistCommand->mergeWith(cmd);
        delete cmd;
    }
    */
}

QIcon PluginSettingsPresenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_CommandLink);
}
}
