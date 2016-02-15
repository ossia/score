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

namespace PluginSettings
{
PluginSettingsPresenter::PluginSettingsPresenter(
        iscore::SettingsDelegateModelInterface& model,
        iscore::SettingsDelegateViewInterface& view,
        QObject* parent) :
    SettingsDelegatePresenterInterface {model, view, parent}
{
    /*
    auto& ps_model = static_cast<PluginSettingsModel&>(model);
    auto& ps_view  = static_cast<PluginSettingsView&>(view);

    ps_view.view()->setModel(ps_model.model());

    con(ps_model,	&PluginSettingsModel::blacklistCommand,
    this,		&PluginSettingsPresenter::setBlacklistCommand);
    */
}


QIcon PluginSettingsPresenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_CommandLink);
}
}
