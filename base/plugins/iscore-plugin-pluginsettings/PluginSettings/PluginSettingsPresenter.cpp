#include <QApplication>
#include <QDebug>
#include <QListView>
#include <QStandardItemModel>
#include <QStyle>

#include "PluginSettingsModel.hpp"
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <iscore/command/Command.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include "PluginSettings/commands/BlacklistCommand.hpp"

namespace iscore {
class SettingsDelegateModel;
class SettingsDelegateView;
class SettingsPresenter;
}  // namespace iscore

namespace PluginSettings
{
PluginSettingsPresenter::PluginSettingsPresenter(
        iscore::SettingsDelegateModel& model,
        iscore::SettingsDelegateView& view,
        QObject* parent) :
    SettingsDelegatePresenter {model, view, parent}
{
    auto& ps_model = static_cast<PluginSettingsModel&>(model);
    auto& ps_view  = static_cast<PluginSettingsView&>(view);

    ps_view.view()->setModel(ps_model.model());
/*
    con(ps_model,	&PluginSettingsModel::blacklistCommand,
    this,		&PluginSettingsPresenter::setBlacklistCommand);*/
}


QIcon PluginSettingsPresenter::settingsIcon()
{
    return QApplication::style()->standardIcon(QStyle::SP_CommandLink);
}
}
