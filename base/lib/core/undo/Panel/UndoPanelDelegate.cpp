// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UndoPanelDelegate.hpp"

#include <QVBoxLayout>
#include <core/document/Document.hpp>
#include <core/undo/Panel/Widgets/UndoListWidget.hpp>

namespace score
{
UndoPanelDelegate::UndoPanelDelegate(const GUIApplicationContext& ctx)
    : PanelDelegate{ctx}, m_widget{new QWidget}
{
  m_widget->setLayout(new QVBoxLayout);
  m_widget->setObjectName("HistoryExplorer");
}

UndoPanelDelegate::~UndoPanelDelegate()
{
}

QWidget* UndoPanelDelegate::widget()
{
  return m_widget;
}

const PanelStatus& UndoPanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false, Qt::LeftDockWidgetArea, 1, QObject::tr("History"),
      QKeySequence::fromString("Ctrl+Shift+H")};

  return status;
}

void UndoPanelDelegate::on_modelChanged(MaybeDocument oldm, MaybeDocument newm)
{
  delete m_list;
  m_list = nullptr;

  if (newm)
  {
    m_list = new score::UndoListWidget{(*newm).document.commandStack()};
    m_widget->layout()->addWidget(m_list);
  }
}
}
