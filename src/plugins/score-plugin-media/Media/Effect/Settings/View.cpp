// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/View.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/Bind.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
namespace Media::Settings
{
View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;

  m_widg->setLayout(lay);

#if defined(HAS_VST2)
  m_VstPaths = new QListWidget;

  auto button_lay = new QHBoxLayout;

  auto pb = new QPushButton{"Add path"};
  auto rescan = new QPushButton{"Rescan"};
  button_lay->addWidget(pb);
  button_lay->addWidget(rescan);

  auto vst_lay = new QGridLayout;
  auto vst_ok = new QListWidget;
  auto vst_bad = new QListWidget;

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

  lay->addRow(tr("VST paths"), m_VstPaths);
  lay->addRow(button_lay);

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
    vst_ok->clear();
    vst_bad->clear();
    for (auto& plug : app_plug.vst_infos)
    {
      if (plug.isValid)
      {
        vst_ok->addItem(
            QString{"%1 - %2\t(%3)"}.arg(plug.prettyName).arg(plug.displayName).arg(plug.path));
      }
      else
      {
        vst_bad->addItem(
            QString{"%1 - %2\t(%3)"}.arg(plug.prettyName).arg(plug.displayName).arg(plug.path));
      }
    }
  };

  reloadVSTs();

  con(app_plug, &Media::ApplicationPlugin::vstChanged, this, reloadVSTs);
  lay->addRow(vst_lay);
  vst_lay->addWidget(new QLabel("Working plug-ins"), 0, 0, 1, 1);
  vst_lay->addWidget(new QLabel("Bad plug-ins"), 0, 1, 1, 1);
  vst_lay->addWidget(vst_ok, 1, 0, 1, 1);
  vst_lay->addWidget(vst_bad, 1, 1, 1, 1);
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
