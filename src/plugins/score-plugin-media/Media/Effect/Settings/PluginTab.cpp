#include "PluginTab.hpp"

#include <score/tools/FilePath.hpp>
#include <score/widgets/FileDialog.hpp>

#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>

namespace Media::Settings
{
namespace
{
QTableWidget* makePluginTable()
{
  auto table = new QTableWidget;
  table->verticalHeader()->setVisible(false);
  table->setColumnCount(2);
  table->setColumnWidth(0, 120);
  table->horizontalHeader()->setStretchLastSection(true);
  table->setHorizontalHeaderLabels(
      {QObject::tr("Name"), QObject::tr("Path")});
  table->setEditTriggers(QAbstractItemView::NoEditTriggers);
  return table;
}

void appendRow(QTableWidget* table, const PluginTabRow& row)
{
  const int r = table->rowCount();
  table->setRowCount(r + 1);
  table->setItem(r, 0, new QTableWidgetItem{row.name});
  table->setItem(r, 1, new QTableWidgetItem{row.path});
}
}

QWidget* makePluginSettingsWidget(PluginTabSpec spec)
{
  auto splitter = new QSplitter(Qt::Vertical);

  // --- Search paths -------------------------------------------------------
  auto pathWidget = new QWidget;
  auto pathLayout = new QFormLayout;
  pathWidget->setLayout(pathLayout);

  auto pathList = new QListWidget;

  auto buttons = new QHBoxLayout;
  auto addPath = new QPushButton{QObject::tr("Add path")};
  auto rescan = new QPushButton{QObject::tr("Rescan")};
  buttons->addWidget(addPath);
  buttons->addWidget(rescan);

  // The current items live in a shared_ptr captured by the handlers: the
  // widget is recreated every time the dialog opens.
  auto items = std::make_shared<QStringList>(spec.getPaths());

  auto setItems = [pathList, items](const QStringList& paths) {
    if(*items == paths && pathList->count() == paths.size())
      return;
    *items = paths;
    pathList->blockSignals(true);
    pathList->clear();
    pathList->addItems(paths);
    pathList->blockSignals(false);
    pathList->update();
  };
  setItems(*items);

  pathList->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
  QObject::connect(
      pathList, &QListWidget::customContextMenuRequested, pathList,
      [pathList, items, commit = spec.commitPaths](const QPoint& p) {
    auto menu = new QMenu;
    auto act = menu->addAction(QObject::tr("Remove"));
    QObject::connect(
        act, &QAction::triggered, pathList, [pathList, items, commit] {
      const int idx = pathList->currentRow();
      if(idx >= 0 && idx < items->size())
      {
        delete pathList->takeItem(idx);
        items->removeAt(idx);
        commit(*items);
      }
        });
    menu->exec(pathList->mapToGlobal(p));
    menu->deleteLater();
      });

  QObject::connect(
      addPath, &QPushButton::clicked, pathList,
      [pathList, items, commit = spec.commitPaths, splitter] {
    QString path;
    if(score::selectExistingDirectory(
           splitter, QObject::tr("Plug-in path"), score::pickerStartFolder({}), path))
    {
      pathList->addItem(path);
      items->push_back(path);
      commit(*items);
    }
      });

  if(spec.onPathsChanged)
    spec.onPathsChanged(pathList, setItems);

  pathLayout->addRow(spec.pathsLabel, pathList);
  pathLayout->addRow(buttons);

  splitter->addWidget(pathWidget);
  splitter->setStretchFactor(0, 1);
  splitter->setCollapsible(0, false);

  // --- Plug-in tables -----------------------------------------------------
  auto ok_table = makePluginTable();
  auto bad_table = makePluginTable();

  auto reload = [ok_table, bad_table, rows = spec.rows] {
    ok_table->clearContents();
    ok_table->setRowCount(0);
    bad_table->clearContents();
    bad_table->setRowCount(0);

    for(const auto& row : rows())
      appendRow(row.valid ? ok_table : bad_table, row);
  };
  reload();

  if(spec.onPluginsChanged)
    spec.onPluginsChanged(ok_table, reload);

  QObject::connect(
      rescan, &QPushButton::clicked, ok_table, [do_rescan = spec.rescan] {
    if(do_rescan)
      do_rescan();
      });

  auto tables = new QWidget;
  auto tables_lay = new QGridLayout;
  tables->setLayout(tables_lay);
  tables_lay->addWidget(new QLabel(QObject::tr("Working plug-ins")), 0, 0, 1, 1);
  tables_lay->addWidget(new QLabel(QObject::tr("Faulty plug-ins")), 0, 1, 1, 1);
  tables_lay->addWidget(ok_table, 1, 0, 1, 1);
  tables_lay->addWidget(bad_table, 1, 1, 1, 1);

  splitter->addWidget(tables);
  splitter->setStretchFactor(1, 4);
  splitter->setCollapsible(1, false);

  return splitter;
}
}
