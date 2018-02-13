// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
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
#include <score/command/Command.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

namespace score
{
class SettingsDelegateModel;
} // namespace score

namespace PluginSettings
{

PluginSettingsPresenter::PluginSettingsPresenter(
    score::SettingsDelegateModel& model,
    score::GlobalSettingsView& view,
    QObject* parent)
    : score::GlobalSettingsPresenter{model, view, parent}
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

        auto it = addon.architectures.find(score::addonArchitecture());
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
