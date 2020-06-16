// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>
#include <score/widgets/FormWidget.hpp>

#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QTableWidget>
#include <QMenu>
#include <QPushButton>
#include <QSplitter>
#include <QHeaderView>

namespace Media::Settings
{
View::View()
{
  m_widg = new score::FormWidget{tr("Effects")};
  auto lay = m_widg->layout();

#if defined(HAS_VST2)
  auto splitter = new QSplitter(Qt::Vertical);
  lay->addRow(splitter);

  auto vstPathWidget = new QWidget;
  auto vstPathWidgetLayout = new QFormLayout;
  vstPathWidget->setLayout(vstPathWidgetLayout);

  m_VstPaths = new QListWidget;

  auto button_lay = new QHBoxLayout;

  auto pb = new QPushButton{"Add path"};
  auto rescan = new QPushButton{"Rescan"};
  button_lay->addWidget(pb);
  button_lay->addWidget(rescan);

  auto vst_lay = new QGridLayout;
  auto vst_ok = new QTableWidget;
  vst_ok->verticalHeader()->setVisible(false);
  vst_ok->setColumnCount(2);
  vst_ok->setColumnWidth(0,120);
  vst_ok->horizontalHeader()->setStretchLastSection(true);
  vst_ok->setHorizontalHeaderLabels({tr("Name"), tr("Path")});

  auto vst_bad = new QTableWidget;
  vst_bad->verticalHeader()->setVisible(false);
  vst_bad->setColumnCount(2);
  vst_bad->setColumnWidth(0,120);
  vst_bad->horizontalHeader()->setStretchLastSection(true);
  vst_bad->setHorizontalHeaderLabels({tr("Name"), tr("Path")});

  m_VstPaths->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
  connect(m_VstPaths, &QListWidget::customContextMenuRequested, this, [=](const QPoint& p) {
    QMenu* m = new QMenu;
    auto act = m->addAction("Remove");
    connect(act, &QAction::triggered, this, [=] {
      auto idx = m_VstPaths->currentRow();

      if (idx >= 0 && idx < m_curitems.size())
      {
        m_VstPaths->takeItem(idx);
        m_curitems.removeAt(idx);
        VstPathsChanged(m_curitems);
      }
    });

    m->exec(m_VstPaths->mapToGlobal(p));
    m->deleteLater();
  });

  vstPathWidgetLayout->addRow(tr("VST paths"), m_VstPaths);
  vstPathWidgetLayout->addRow(button_lay);

  splitter->addWidget(vstPathWidget);
  splitter->setStretchFactor(0,1);
  splitter->setCollapsible(0,false);

  connect(pb, &QPushButton::clicked, this, [=] {
    auto path = QFileDialog::getExistingDirectory(m_widg, tr("VST Path"));
    if (!path.isEmpty())
    {
      m_VstPaths->addItem(path);
      m_curitems.push_back(path);
      VstPathsChanged(m_curitems);
    }
  });
  auto& app_plug = score::GUIAppContext().applicationPlugin<Media::ApplicationPlugin>();

  connect(rescan, &QPushButton::clicked, this, [&] { app_plug.rescanVSTs(m_curitems); });

  auto reloadVSTs = [=, &app_plug] {
    vst_ok->clearContents();
    vst_ok->setRowCount(0);

    vst_bad->clearContents();
    vst_bad->setRowCount(0);

    for (auto& plug : app_plug.vst_infos)
    {
      if (plug.isValid)
      {
        auto row = vst_ok->rowCount();
        vst_ok->setRowCount( row + 1);
        vst_ok->setItem(row, 0, new QTableWidgetItem{plug.prettyName.isEmpty() ? plug.displayName : plug.prettyName});
        vst_ok->setItem(row, 1, new QTableWidgetItem{plug.path});
      }
      else
      {
        auto row = vst_bad->rowCount();
        vst_bad->setRowCount( row + 1);
        vst_bad->setItem(row, 0, new QTableWidgetItem{plug.prettyName.isEmpty() ? plug.displayName : plug.prettyName});
        vst_bad->setItem(row, 1, new QTableWidgetItem{plug.path});
      }
    }
  };

  reloadVSTs();

  auto vstWidget = new QWidget;
  vstWidget->setLayout(vst_lay);

  con(app_plug, &Media::ApplicationPlugin::vstChanged, this, reloadVSTs);
  vst_lay->addWidget(new QLabel(tr("Working plug-ins")), 0, 0, 1, 1);
  vst_lay->addWidget(new QLabel(tr("Faulty plug-ins")), 0, 1, 1, 1);
  vst_lay->addWidget(vst_ok, 1, 0, 1, 1);
  vst_lay->addWidget(vst_bad, 1, 1, 1, 1);

  splitter->addWidget(vstWidget);
  splitter->setStretchFactor(1,4);
  splitter->setCollapsible(1,false);

#endif
}

void View::setVstPaths(QStringList val)
{
#if defined(HAS_VST2)
  if (m_curitems != val)
  {
    m_curitems = val;
    m_VstPaths->blockSignals(true);
    m_VstPaths->clear();
    m_VstPaths->addItems(val);
    m_VstPaths->blockSignals(false);
    m_VstPaths->update();
  }
#endif
}

QWidget* View::getWidget()
{
  return m_widg;
}
}
