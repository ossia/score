// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UndoPanelDelegate.hpp"

#include <core/document/Document.hpp>
#include <core/undo/Panel/Widgets/UndoListWidget.hpp>
#include <score/widgets/MarginLess.hpp>

#include <QVBoxLayout>

namespace score
{
UndoPanelDelegate::UndoPanelDelegate(const GUIApplicationContext& ctx)
    : PanelDelegate{ctx}, m_widget{new QWidget}
{
  m_widget->setLayout(new score::MarginLess<QVBoxLayout>);
  m_widget->setObjectName("HistoryExplorer");
}

UndoPanelDelegate::~UndoPanelDelegate() {}

QWidget* UndoPanelDelegate::widget()
{
  return m_widget;
}

const PanelStatus& UndoPanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      false,
      false,
      Qt::LeftDockWidgetArea,
      20,
      QObject::tr("History"),
      "history",
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
