#include <QBoxLayout>
#include <QLayout>
#include <qnamespace.h>
#include <QObject>
#include <QWidget>

#include "UndoView.hpp"
#include "Widgets/UndoListWidget.hpp"
#include <iscore/plugins/panel/PanelView.hpp>

static const iscore::DefaultPanelStatus status{true, Qt::LeftDockWidgetArea, 1, QObject::tr("History")};

const iscore::DefaultPanelStatus &UndoView::defaultPanelStatus() const
{ return status; }

UndoView::UndoView(QObject* v) :
    iscore::PanelView{v},
    m_widget{new QWidget}
{
    m_widget->setLayout(new QVBoxLayout);
    m_widget->setObjectName("HistoryExplorer");
}


QWidget*UndoView::getWidget()
{
    return m_widget;
}

void UndoView::setStack(iscore::CommandStack* s)
{
    delete m_list;
    m_list = nullptr;

    if(s)
    {
        m_list = new iscore::UndoListWidget{s};
        m_widget->layout()->addWidget(m_list);
    }
}
