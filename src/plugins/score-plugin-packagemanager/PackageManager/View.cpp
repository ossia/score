// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "View.hpp"

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationInterface.hpp>

#include <ossia/detail/ssize.hpp>

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include <PackageManager/FileDownloader.hpp>
#include <PackageManager/Presenter.hpp>

#include <score_git_info.hpp>
#include <wobjectimpl.h>
#include <zipdownloader.hpp>

W_OBJECT_IMPL(PM::PluginSettingsView)
namespace PM
{
namespace zip_helper
{

QString get_path(const QString& str)
{
  auto idx = str.lastIndexOf('/');
  if(idx != -1)
  {
    return str.mid(0, idx);
  }
  return "";
}

QString slash_path(const QString& str)
{
  return {};
}

QString relative_path(const QString& base, const QString& filename)
{
  return filename;
}

QString combine_path(const QString& path, const QString& filename)
{
  return path + "/" + filename;
}

bool make_folder(const QString& str)
{
  QDir d;
  return d.mkpath(str);
}

}

void PluginSettingsView::show_progress()
{
  m_progress->setVisible(true);
}

void PluginSettingsView::update_progress(double v)
{
  m_progress->setValue(100. * v); //m_progress->value() + (100.0 / m_addonsToRetrieve));
}

PluginSettingsView::PluginSettingsView()
{
  storage = QStorageInfo(
      score::AppContext().settings<Library::Settings::Model>().getPackagesPath());

  m_progress->setMinimum(0);
  m_progress->setMaximum(100);
  m_progress->setHidden(true);

  auto grid = new QGridLayout{m_widget};
  grid->setContentsMargins(0, 0, 0, 0);
  m_widget->setLayout(grid);

  auto tab_widget = new QTabWidget;
  grid->addWidget(tab_widget, 0, 0);
  grid->addWidget(m_progress, 1, 0);

  {
    auto local_widget = new QWidget;
    auto local_layout = new QVBoxLayout{local_widget};
    local_widget->setLayout(local_layout);
    local_layout->addWidget(m_addonsOnSystem);

    tab_widget->addTab(local_widget, tr("Local packages"));
  }

  {
    auto remote_widget = new QWidget;
    auto remote_layout = new QVBoxLayout{remote_widget};
    remote_widget->setLayout(remote_layout);
    remote_layout->addWidget(m_remoteAddons);

    tab_widget->addTab(remote_widget, tr("Available packages"));
  }

  auto side_widget = new QWidget;
  auto vlay = new QVBoxLayout{side_widget};
  grid->addWidget(side_widget, 0, 1, 2, 1);

  auto categoryLabel = new QLabel{tr("Filter by kind:")};
  vlay->addWidget(categoryLabel);

  m_categoryComboBox = new QComboBox;
  vlay->addWidget(m_categoryComboBox);
  vlay->addSpacing(20);

  m_link->setToolTip(tr("Open external package link in default browser."));
  auto icon = makeIcons(
      QStringLiteral(":/icons/undock_on.png"), QStringLiteral(":/icons/undock_off.png"),
      QStringLiteral(":/icons/undock_off.png"));
  m_link->setIcon(icon);

  vlay->addWidget(m_link);

  vlay->addSpacing(20);

  vlay->addWidget(m_uninstall);
  m_install->setVisible(false);
  vlay->addWidget(m_install);
  vlay->addSpacing(20);

  vlay->addWidget(m_update);
  vlay->addWidget(m_updateAll);
  vlay->addStretch();

  set_info();
  vlay->addWidget(m_storage);

  for(QTableView* v : {m_addonsOnSystem, m_remoteAddons})
  {
    v->verticalHeader()->hide();
    v->verticalHeader()->sectionResizeMode(QHeaderView::Fixed);
    v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setShowGrid(false);
  }

  connect(tab_widget, &QTabWidget::tabBarClicked, this, [this](int i) {
    if(i == 1) // Remote
    {
      m_uninstall->setVisible(false);
      m_install->setVisible(true);
      m_update->setVisible(false);
      m_updateAll->setVisible(false);

      RemotePackagesModel* model
          = static_cast<RemotePackagesModel*>(m_remoteAddons->model());
      model->clear();

      m_progress->setVisible(true);
      m_progress->setValue(0);

      refresh();
      updateCategoryFilter();
    }
    else // Local
    {
      m_uninstall->setVisible(true);
      m_install->setVisible(false);
      m_update->setVisible(true);
      m_updateAll->setVisible(true);
      updateCategoryFilter();
    }
  });

  connect(m_link, &QPushButton::pressed, this, &PluginSettingsView::openLink);

  connect(m_uninstall, &QPushButton::pressed, this, &PluginSettingsView::uninstall);

  connect(m_install, &QPushButton::pressed, this, &PluginSettingsView::install);

  connect(m_update, &QPushButton::pressed, this, &PluginSettingsView::update);

  connect(m_updateAll, &QPushButton::pressed, this, &PluginSettingsView::updateAll);

  connect(
      m_categoryComboBox, &QComboBox::currentIndexChanged, this,
      &PluginSettingsView::applyCategoryFilter);

  refresh();
}

QWidget* PluginSettingsView::getWidget()
{
  return m_widget;
}

// the install button set to visible means we are browsing
PackagesModel* PluginSettingsView::getCurrentModel()
{
  if(m_install->isVisible())
    return static_cast<PackagesModel*>(m_remoteAddons->model());
  else
    return static_cast<PackagesModel*>(m_addonsOnSystem->model());
}

int PluginSettingsView::getCurrentRow(const QTableView* t = nullptr)
{
  QModelIndexList rows{};

  if(t)
    rows = t->selectionModel()->selectedRows(0);
  else
  {
    if(m_install->isVisible())
      rows = m_remoteAddons->selectionModel()->selectedRows(0);
    else
      rows = m_addonsOnSystem->selectionModel()->selectedRows(0);
  }

  if(rows.isEmpty())
    return -1;

  return rows.first().row();
}

Package PluginSettingsView::selectedPackage(const PackagesModel* model, int row)
{
  if(row == -1)
    return {};

  SCORE_ASSERT(int(model->addons().size()) > row);

  return model->addons().at(row);
}

void PluginSettingsView::openLink()
{
  const auto& addon = selectedPackage(getCurrentModel(), getCurrentRow());

  QDesktopServices::openUrl(addon.url);
}

void PluginSettingsView::install_package(const Package& addon)
{
  if(addon.kind == "addon" || addon.kind == "nodes")
    installAddon(addon);
  else if(addon.kind == "sdk")
    installSDK();
  else
    installLibrary(addon);
}

void PluginSettingsView::install()
{
  const auto& addon = selectedPackage(
      static_cast<PackagesModel*>(m_remoteAddons->model()),
      getCurrentRow(m_remoteAddons));

  m_progress->setVisible(true);

  install_package(addon);
}

void PluginSettingsView::uninstall()
{
  const auto& addon = selectedPackage(
      static_cast<PackagesModel*>(m_addonsOnSystem->model()),
      getCurrentRow(m_addonsOnSystem));

  bool success{false};

  const auto& library{score::AppContext().settings<Library::Settings::Model>()};

  if(addon.kind == "sdk")
  {
    success = QDir{library.getSDKPath()}.removeRecursively();
  }
  else if(!addon.raw_name.isEmpty())
  {
    // Mirrors the install paths used by PluginSettingsModel::installAddon and
    // installLibrary: every non-SDK kind is extracted into <path>/<raw_name>.
    // Guarding on raw_name matters: with no selection selectedPackage() returns a
    // default-constructed Package, and removing <path>/ would wipe the library.
    const QString& installPath
        = addon.kind == "support" ? library.getSupportPath() : library.getPackagesPath();
    success = QDir{installPath + '/' + addon.raw_name}.removeRecursively();
  }

  if(success)
  {
    const auto& localPlugins
        = static_cast<LocalPackagesModel*>(m_addonsOnSystem->model());

    localPlugins->removeAddon(addon);
    set_info();
  }
}

void PluginSettingsView::update()
{
  auto local_model = static_cast<PackagesModel*>(m_addonsOnSystem->model());
  auto remote_model = static_cast<PackagesModel*>(m_remoteAddons->model());
  const auto& addon = selectedPackage(local_model, getCurrentRow(m_addonsOnSystem));

  auto key = addon.key;
  auto it = ossia::find_if(
      remote_model->addons(), [&](auto& pkg) { return pkg.key == addon.key; });
  if(it == remote_model->addons().end())
  {
    qDebug() << "Addon " << addon.name << "not found on the server!";
    return;
  }

  m_progress->setVisible(true);
  install_package(*it);
}

void PluginSettingsView::updateAll()
{
  auto local_model = static_cast<PackagesModel*>(m_addonsOnSystem->model());
  auto remote_model = static_cast<PackagesModel*>(m_remoteAddons->model());

  for(auto& addon : local_model->addons())
  {
    auto key = addon.key;
    auto it = ossia::find_if(
        remote_model->addons(), [&](auto& pkg) { return pkg.key == addon.key; });
    if(it == remote_model->addons().end())
    {
      qDebug() << "Addon " << addon.name << "not found on the server!";
      continue;
    }

    if(it->version <= addon.version)
      continue;

    m_progress->setVisible(true);
    install_package(*it);
  }
}

void PluginSettingsView::set_info()
{
  m_storage->setText(
      QString::number(storage.bytesAvailable() / 1024.0 / 1024.0 / 1024) + " G\n"
      + tr("available on volume"));
};

void PluginSettingsView::reset_progress()
{
  m_progress->setHidden(true);
  m_progress->setValue(0);
}

void PluginSettingsView::progress_from_bytes(qint64 bytesReceived, qint64 bytesTotal)
{
  m_progress->setValue(((bytesReceived / 1024.) / (bytesTotal / 1024.)) * 100);
}

// Same rule as getCurrentModel(): the install button is only shown while browsing.
QTableView* PluginSettingsView::getCurrentView()
{
  return m_install->isVisible() ? m_remoteAddons : m_addonsOnSystem;
}

void PluginSettingsView::updateCategoryFilter()
{
  updateCategoryComboBox();
  applyCategoryFilter();
}

// Rebuild the list of kinds from the packages currently in the active model.
// Packages trickle in asynchronously, so this runs again on every model reset:
// keep the user's choice selected instead of snapping back to "All".
void PluginSettingsView::updateCategoryComboBox()
{
  const QString previous = m_categoryComboBox->currentData().toString();

  // Rebuilding would otherwise re-trigger the filter on every intermediate state.
  const QSignalBlocker blocker{m_categoryComboBox};

  m_categoryComboBox->clear();
  m_categoryComboBox->addItem(tr("All"), QString{});

  QStringList kinds;
  if(auto* model = getCurrentModel())
  {
    for(const auto& addon : model->addons())
    {
      if(!addon.kind.isEmpty() && !kinds.contains(addon.kind))
        kinds.push_back(addon.kind);
    }
    // Packages arrive in network order: sort so the list does not jump around.
    kinds.sort();
  }

  for(const QString& kind : kinds)
    m_categoryComboBox->addItem(kind, kind);

  const int index = previous.isEmpty() ? 0 : m_categoryComboBox->findData(previous);
  m_categoryComboBox->setCurrentIndex(index >= 0 ? index : 0);
}

// Note: the models reset themselves whenever a package is added, which clears the
// views' hidden-row state - hence re-applying the filter on every change.
void PluginSettingsView::applyCategoryFilter()
{
  auto* model = getCurrentModel();
  if(!model)
    return;

  QTableView* view = getCurrentView();

  // Empty means "All": either the first entry, or an empty/-1 current index.
  const QString kind = m_categoryComboBox->currentData().toString();

  const int rows = std::ssize(model->addons());
  for(int row = 0; row < rows; ++row)
    view->setRowHidden(row, !kind.isEmpty() && model->addons()[row].kind != kind);

  // Do not leave a hidden package selected: install / uninstall would then act
  // on something the user cannot see.
  if(auto* selection = view->selectionModel())
  {
    const int current = selection->currentIndex().row();
    if(current >= 0 && current < rows && view->isRowHidden(current))
    {
      selection->clearSelection();
      selection->clearCurrentIndex();
    }
  }
}

}
