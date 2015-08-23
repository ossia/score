#include "StateWidget.hpp"
#include <State/State.hpp>
#include <State/Message.hpp>
#include <State/ProcessState.hpp>

#include <QGridLayout>
#include <QToolButton>
#include <QLabel>
#include "MessageWidget.hpp"

using namespace iscore;
StateWidget::StateWidget(
        const State& state,
        const CommandDispatcher<>& disp,
        QWidget* parent):
    QFrame{parent}
{
    this->setFrameShape(QFrame::StyledPanel);
    auto lay = new QGridLayout{this};

    lay->addWidget(new QLabel{tr("State")}, 0, 0);

    QToolButton* rmBtn = new QToolButton;
    rmBtn->setText(tr("Remove"));
    lay->addWidget(rmBtn, 0, 1);

    connect(rmBtn, &QToolButton::clicked,
            this,  &StateWidget::removeMe);

    if(state.data().canConvert<State>())
    {
        lay->addWidget(new StateWidget{state.data().value<State>(), disp, this});
    }
    else if(state.data().canConvert<StateList>())
    {
        for(const State& s : state.data().value<StateList>())
        {
            lay->addWidget(new StateWidget {s, disp, this});
        }
    }
    else if(state.data().canConvert<Message>())
    {
        lay->addWidget(new MessageWidget {state.data().value<Message>(), disp, this});
    }
    else if(state.data().canConvert<MessageList>())
    {
        int i = 0;
        for(const Message& mess : state.data().value<MessageList>())
        {
            lay->addWidget(new MessageWidget {mess, disp, this}, ++i, 0);
        }
    }
    else if(state.data().canConvert<ProcessState>())
    {
        ISCORE_TODO;
    }
}
