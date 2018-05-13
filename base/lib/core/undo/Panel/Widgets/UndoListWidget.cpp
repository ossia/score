// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "UndoListWidget.hpp"

#include <QItemSelectionModel>
#include <core/command/CommandStack.hpp>
#include <score/command/Command.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::UndoListWidget)
namespace score
{
UndoListWidget::UndoListWidget(score::CommandStack& s) : m_stack{s}
{
  on_stackChanged();

  con(m_stack, &score::CommandStack::stackChanged, this,
      &score::UndoListWidget::on_stackChanged);
  connect(
      this, &QListWidget::currentRowChanged, &m_stack,
      &CommandStack::setIndex);
}

UndoListWidget::~UndoListWidget() = default;

void UndoListWidget::on_stackChanged()
{
  this->blockSignals(true);
  clear();
  addItem("<Clean state>");
  for (int i = 0; i < m_stack.size(); i++)
  {
    auto cmd = m_stack.command(i);
    addItem(cmd->description());
  }

  this->setCurrentRow(
      m_stack.currentIndex(),
      QItemSelectionModel::SelectionFlag::ClearAndSelect);

  this->blockSignals(false);
}
}
