#include "ConstraintSummaryWidget.hpp"

#include <iscore/document/DocumentContext.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QLabel>
#include <QGridLayout>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Inspector/SelectionButton.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Process/Process.hpp>

namespace Scenario
{
ConstraintSummaryWidget::ConstraintSummaryWidget(const ConstraintModel& object,
                         const iscore::DocumentContext& doc,
                         QWidget *parent) :
    QWidget(parent),
    m_selectionDispatcher{new iscore::SelectionDispatcher{doc.selectionStack}}
{
    auto mainLay = new iscore::MarginLess<QGridLayout>{this};

    auto eventBtn = SelectionButton::make("", &object, *m_selectionDispatcher , this);

    mainLay->addWidget(new QLabel{object.metadata.name()},
                       0, 0, 1, 3);
    mainLay->addWidget(new QLabel{tr("start : ") + object.startDate().toString()},
                       0, 3, 1, 3);
    mainLay->addWidget(eventBtn,
                       0, 6, 1, 1);


    if(object.duration.isRigid())
    {
        mainLay->addWidget(new QLabel{object.duration.defaultDuration().toString()},
                           1, 1, 1, 4);
    }
    else
    {
        QString max = object.duration.maxDuration().isInfinite() ? "inf" : object.duration.maxDuration().toString();
        QString min = object.duration.minDuration().isZero() ? "0" : object.duration.minDuration().toString();
        mainLay->addWidget(new QLabel{tr("Flexible : ")
                                      + min
                                      + " to "
                                      + max},
                           1, 1, 1, 4);

    }


    if(!object.processes.empty())
    {
        auto processList = new Inspector::InspectorSectionWidget{tr("processes"), false, this};
        for(const auto& p : object.processes)
        {
            processList->addContent(new QLabel{p.prettyName()});
        }
        mainLay->addWidget(processList, 2, 1, 1, 6);
    }

}
}
