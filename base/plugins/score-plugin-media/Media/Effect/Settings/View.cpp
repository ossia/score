// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/View.hpp>
#include <QListWidget>
#include <QPushButton>
#include <QFileDialog>
#include <QMenu>
#include <QFormLayout>
#include <score/widgets/SignalUtils.hpp>
namespace Media::Settings
{
View::View() : m_widg{new QWidget}
{
  auto lay = new QFormLayout;

  m_widg->setLayout(lay);

  m_VstPaths = new QListWidget;
  m_VstPaths->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
  connect(m_VstPaths, &QListWidget::customContextMenuRequested,
          this, [=] (const QPoint& p) {
    QMenu* m = new QMenu;
    auto act = m->addAction("Remove");
    connect(act, &QAction::triggered,
            this, [=] {
      auto idx = m_VstPaths->currentRow();

      if(idx >= 0 && idx < m_curitems.size())
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
  auto pb = new QPushButton{"Add path"};
  lay->addWidget(pb);
  connect(pb, &QPushButton::clicked,
          this, [=] {
    auto path = QFileDialog::getExistingDirectory(m_widg, tr("VST Path"));
    if(!path.isEmpty())
    {
      m_VstPaths->addItem(path);
      m_curitems.push_back(path);
      VstPathsChanged(m_curitems);
    }
  });
}

void View::setVstPaths(QStringList val)
{
  if(m_curitems != val)
  {
    m_curitems = val;
    m_VstPaths->blockSignals(true);
    m_VstPaths->clear();
    m_VstPaths->addItems(val);
    m_VstPaths->blockSignals(false);
    m_VstPaths->update();
  }
}

QWidget* View::getWidget()
{
  return m_widg;
}

}
