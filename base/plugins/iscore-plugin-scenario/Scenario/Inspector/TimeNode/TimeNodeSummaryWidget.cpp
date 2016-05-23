#include "TimeNodeSummaryWidget.hpp"

#include <iscore/document/DocumentContext.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QLabel>
#include <QGridLayout>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>
#include <State/Expression.hpp>

namespace Scenario
{
TimeNodeSummaryWidget::TimeNodeSummaryWidget(const TimeNodeModel& object,
                         const iscore::DocumentContext& doc,
                         QWidget *parent) :
    QWidget(parent),
    m_selectionDispatcher{new iscore::SelectionDispatcher{doc.selectionStack}}
{
    auto mainLay = new iscore::MarginLess<QGridLayout>{this};

    auto eventBtn = SelectionButton::make("", &object, *m_selectionDispatcher , this);

    mainLay->addWidget(new QLabel{object.metadata.name()},0, 0, 1, 3);
    mainLay->addWidget( new QLabel{object.date().toString()}, 0, 3, 1, 3);
    mainLay->addWidget(eventBtn, 0, 6, 1, 1);

    if(!object.expression().isEmpty())
    {
    auto cond = new QLabel{object.expression()};
    cond->setWordWrap(true);
    mainLay->addWidget(cond, 1, 1, 1, 6);
    }

}
}
