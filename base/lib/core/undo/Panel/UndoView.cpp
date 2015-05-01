#include "UndoView.hpp"

#include <QVBoxLayout>
#include "Widgets/UndoListWidget.hpp"


UndoView::UndoView(QObject* v) :
    iscore::PanelViewInterface{v},
    m_widget{new QWidget}
{
    m_widget->setLayout(new QVBoxLayout);
}


QWidget*UndoView::getWidget()
{
    return m_widget;
}


Qt::DockWidgetArea UndoView::defaultDock() const
{
    return Qt::LeftDockWidgetArea;
}

int UndoView::priority() const
{
    return 0;
}

QString UndoView::prettyName() const
{
    return tr("History");
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
