// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Presenter.hpp"

#include "Model.hpp"
#include "View.hpp"

#include <score/command/Command.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/widgets/SetIcons.hpp>

#include <QApplication>
#include <QDebug>
#include <QHeaderView>
#include <QListView>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QStyle>

#include <PackageManager/FileDownloader.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(PM::PluginSettingsPresenter)

namespace score
{
class SettingsDelegateModel;
} // namespace score

namespace PM
{

PluginSettingsPresenter::PluginSettingsPresenter(
    score::SettingsDelegateModel& model, score::GlobalSettingsView& view,
    QObject* parent)
    : score::GlobalSettingsPresenter{model, view, parent}
{
  auto& ps_model = static_cast<PluginSettingsModel&>(model);
  auto& ps_view = static_cast<PluginSettingsView&>(view);

  ps_view.localView()->setModel(&ps_model.localPlugins);
  ps_view.localView()->setColumnWidth(0, 200);
  ps_view.localView()->setColumnWidth(1, 40);
  ps_view.localView()->setColumnWidth(2, 40);
  ps_view.localView()->horizontalHeader()->setStretchLastSection(true);

  ps_view.remoteView()->setModel(&ps_model.remotePlugins);
  ps_view.remoteView()->setColumnWidth(0, 200);
  ps_view.remoteView()->setColumnWidth(1, 40);
  ps_view.remoteView()->setColumnWidth(2, 40);
  ps_view.remoteView()->horizontalHeader()->setStretchLastSection(true);

  ps_view.remoteView()->setSelectionModel(&ps_model.remoteSelection);

  connect(
      &ps_model.remoteSelection, &QItemSelectionModel::currentRowChanged, this,
      [&](const QModelIndex& current, const QModelIndex& previous) {
    Package& addon = ps_model.remotePlugins.addons().at(current.row());

    ps_view.installButton().setEnabled(!addon.files.empty() || addon.kind == "sdk");
      });

  ps_view.installButton().setEnabled(false);

  /*
      con(ps_model,	&PluginSettingsModel::blacklistCommand,
      this,		&PluginSettingsPresenter::setBlacklistCommand);*/

  connect(
      &ps_model, &PluginSettingsModel::show_progress, &ps_view,
      &PluginSettingsView::show_progress);
  connect(
      &ps_model, &PluginSettingsModel::update_progress, &ps_view,
      &PluginSettingsView::update_progress);
  connect(
      &ps_model, &PluginSettingsModel::reset_progress, &ps_view,
      &PluginSettingsView::reset_progress);
  connect(
      &ps_model, &PluginSettingsModel::set_info, &ps_view,
      &PluginSettingsView::set_info);
  connect(
      &ps_model, &PluginSettingsModel::progress_from_bytes, &ps_view,
      &PluginSettingsView::progress_from_bytes);
  connect(
      &ps_model, &PluginSettingsModel::information, &ps_view,
      [w = ps_view.getWidget()](const QString& t, const QString& d) {
    QMessageBox::information(w, t, d);
  });
  connect(
      &ps_model, &PluginSettingsModel::warning, &ps_view,
      [w = ps_view.getWidget()](const QString& t, const QString& d) {
    QMessageBox::warning(w, t, d);
  });

  connect(
      &ps_view, &PluginSettingsView::refresh, &ps_model, &PluginSettingsModel::refresh);
  connect(
      &ps_view, &PluginSettingsView::requestInformation, &ps_model,
      &PluginSettingsModel::requestInformation);
  connect(
      &ps_view, &PluginSettingsView::installAddon, &ps_model,
      &PluginSettingsModel::installAddon);
  connect(
      &ps_view, &PluginSettingsView::installLibrary, &ps_model,
      &PluginSettingsModel::installLibrary);
  connect(
      &ps_view, &PluginSettingsView::installSDK, &ps_model,
      &PluginSettingsModel::installSDK);
}

QIcon PluginSettingsPresenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_package_on.png"),
      QStringLiteral(":/icons/settings_package_off.png"),
      QStringLiteral(":/icons/settings_package_off.png"));
}
}
