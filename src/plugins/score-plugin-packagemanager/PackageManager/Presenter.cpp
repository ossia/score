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
#include <QSortFilterProxyModel>
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

  ps_view.m_remoteModel = &ps_model.remotePlugins;
  ps_view.m_localModel = &ps_model.localPlugins;

  QSortFilterProxyModel* localProxyModel = new QSortFilterProxyModel(this);
  localProxyModel->setSourceModel(&ps_model.localPlugins);
  localProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  QSortFilterProxyModel* remoteProxyModel = new QSortFilterProxyModel(this);
  remoteProxyModel->setSourceModel(&ps_model.remotePlugins);
  remoteProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);

  ps_view.localView()->setModel(localProxyModel);
  ps_view.localView()->setColumnWidth(0, 200);
  ps_view.localView()->setColumnWidth(1, 40);
  ps_view.localView()->setColumnWidth(2, 40);
  ps_view.localView()->setColumnWidth(3, 300);
  ps_view.localView()->setColumnWidth(4, 30);
  ps_view.localView()->horizontalHeader()->setStretchLastSection(true);

  ps_view.remoteView()->setModel(remoteProxyModel);
  ps_view.remoteView()->setColumnWidth(0, 200);
  ps_view.remoteView()->setColumnWidth(1, 50);
  ps_view.remoteView()->setColumnWidth(2, 50);
  ps_view.remoteView()->setColumnWidth(3, 300);
  ps_view.remoteView()->setColumnWidth(4, 30);
  ps_view.remoteView()->horizontalHeader()->setStretchLastSection(true);

  // ps_view.remoteView()->setSelectionModel(&ps_model.remoteSelection);
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
}

QIcon PluginSettingsPresenter::settingsIcon()
{
  return makeIcons(
      QStringLiteral(":/icons/settings_package_on.png"),
      QStringLiteral(":/icons/settings_package_off.png"),
      QStringLiteral(":/icons/settings_package_off.png"));
}
}
