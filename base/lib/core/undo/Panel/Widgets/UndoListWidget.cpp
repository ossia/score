#include <core/command/CommandStack.hpp>
#include <QItemSelectionModel>

#include "UndoListWidget.hpp"
#include <iscore/command/SerializableCommand.hpp>

namespace iscore
{
UndoListWidget::UndoListWidget(iscore::CommandStack& s):
    m_stack{s}
{
    on_stackChanged();

    con(m_stack,&iscore::CommandStack::stackChanged,
        this, &iscore::UndoListWidget::on_stackChanged);
    connect(this, &QListWidget::currentRowChanged,
            &m_stack, &CommandStack::setIndex);
}

UndoListWidget::~UndoListWidget() = default;

void UndoListWidget::on_stackChanged()
{
    this->blockSignals(true);
    clear();
    addItem("<Clean state>");
    for(int i = 0; i < m_stack.size(); i++)
    {
        auto cmd = m_stack.command(i);
        addItem(cmd->description());
    }

    this->setCurrentRow(m_stack.currentIndex(), QItemSelectionModel::SelectionFlag::ClearAndSelect);

    this->blockSignals(false);
}
}
