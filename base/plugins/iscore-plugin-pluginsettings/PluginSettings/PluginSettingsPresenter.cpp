#include <QApplication>
#include <QDebug>
#include <QListView>
#include <QStandardItemModel>
#include <QStyle>

#include "PluginSettings/commands/BlacklistCommand.hpp"
#include "PluginSettingsModel.hpp"
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <PluginSettings/FileDownloader.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

namespace iscore
{
class SettingsDelegateModel;
class SettingsDelegateView;
class SettingsPresenter;
} // namespace iscore

namespace PluginSettings
{

PluginSettingsPresenter::PluginSettingsPresenter(
    iscore::SettingsDelegateModel& model,
    iscore::SettingsDelegateView& view,
    QObject* parent)
    : SettingsDelegatePresenter{model, view, parent}
{
  auto& ps_model = static_cast<PluginSettingsModel&>(model);
  auto& ps_view = static_cast<PluginSettingsView&>(view);

  ps_view.localView()->setModel(&ps_model.localPlugins);
  ps_view.localView()->setColumnWidth(0, 150);
  ps_view.localView()->setColumnWidth(1, 400);
  ps_view.localView()->setColumnWidth(2, 400);

  ps_view.remoteView()->setModel(&ps_model.remotePlugins);
  ps_view.remoteView()->setColumnWidth(0, 150);
  ps_view.remoteView()->setColumnWidth(1, 400);
  ps_view.remoteView()->setSelectionModel(&ps_model.remoteSelection);

  connect(
      &ps_model.remoteSelection, &QItemSelectionModel::currentRowChanged, this,
      [&](const QModelIndex& current, const QModelIndex& previous) {

        RemoteAddon& addon = ps_model.remotePlugins.addons().at(current.row());

        auto it = addon.architectures.find(iscore::addonArchitecture());
        bool b = (it != addon.architectures.end()) && (it->second != QUrl{});

        ps_view.installButton().setEnabled(b);
      });

  ps_view.installButton().setEnabled(false);

  /*
      con(ps_model,	&PluginSettingsModel::blacklistCommand,
      this,		&PluginSettingsPresenter::setBlacklistCommand);*/
}

QIcon PluginSettingsPresenter::settingsIcon()
{
  return QApplication::style()->standardIcon(QStyle::SP_CommandLink);
}
}
