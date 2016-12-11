#include "UndoPanelDelegate.hpp"

#include <QVBoxLayout>
#include <core/document/Document.hpp>
#include <core/undo/Panel/Widgets/UndoListWidget.hpp>

namespace iscore
{
UndoPanelDelegate::UndoPanelDelegate(const GUIApplicationContext& ctx)
    : PanelDelegate{ctx}, m_widget{new QWidget}
{
  m_widget->setLayout(new QVBoxLayout);
  m_widget->setObjectName("HistoryExplorer");
}

QWidget* UndoPanelDelegate::widget()
{
  return m_widget;
}

const PanelStatus& UndoPanelDelegate::defaultPanelStatus() const
{
  static const iscore::PanelStatus status{
      true, Qt::LeftDockWidgetArea, 1, QObject::tr("History"),
      QKeySequence::fromString("Ctrl+Shift+H")};

  return status;
}

void UndoPanelDelegate::on_modelChanged(MaybeDocument oldm, MaybeDocument newm)
{
  delete m_list;
  m_list = nullptr;

  if (newm)
  {
    m_list = new iscore::UndoListWidget{(*newm).document.commandStack()};
    m_widget->layout()->addWidget(m_list);
  }
}
}
