#include "UndoListWidget.hpp"
#include <core/command/CommandStack.hpp>
using namespace iscore;

UndoListWidget::UndoListWidget(iscore::CommandStack* s):
    m_stack{s}
{
    on_stackChanged();

    connect(m_stack,&iscore::CommandStack::stackChanged,
            this, &iscore::UndoListWidget::on_stackChanged);
    connect(this, &QListWidget::currentRowChanged,
            m_stack, &CommandStack::setIndex);
}

UndoListWidget::~UndoListWidget()
{
}


void UndoListWidget::on_stackChanged()
{
    clear();
    addItem("<Clean state>");
    for(int i = 0; i < m_stack->size(); i++)
    {
        auto cmd = m_stack->command(i);
        addItem(cmd->text());
    }

    this->setCurrentRow(m_stack->currentIndex(), QItemSelectionModel::SelectionFlag::ClearAndSelect);
}
