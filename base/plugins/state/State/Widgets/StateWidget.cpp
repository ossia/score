#include "StateWidget.hpp"
#include <State/State.hpp>
#include <State/Message.hpp>
#include <State/ProcessState.hpp>

#include <QGridLayout>
#include <QToolButton>
#include "MessageWidget.hpp"

StateWidget::StateWidget(const State& state, QWidget* parent):
    QFrame{parent}
{
    this->setFrameShape(QFrame::StyledPanel);
    auto lay = new QGridLayout{this};

    lay->addWidget(new QLabel{tr("State")}, 0, 0);

    QToolButton* rmBtn = new QToolButton;
    rmBtn->setText("X");
    lay->addWidget(rmBtn, 0, 1);

    connect(rmBtn, &QToolButton::clicked,
            this,  &StateWidget::removeMe);

    if(state.data().canConvert<State>())
    {
        lay->addWidget(new StateWidget{state.data().value<State>(), this});
    }
    else if(state.data().canConvert<StateList>())
    {
        for(const State& s : state.data().value<StateList>())
        {
            lay->addWidget(new StateWidget {s, this});
        }
    }
    else if(state.data().canConvert<Message>())
    {
        lay->addWidget(new MessageWidget {state.data().value<Message>(), this});
    }
    else if(state.data().canConvert<MessageList>())
    {
        // TODO Check removal in this case.
        int i = 0;
        for(const Message& mess : state.data().value<MessageList>())
        {
            lay->addWidget(new MessageWidget {mess, this}, ++i, 0);
        }
    }
    else if(state.data().canConvert<ProcessState>())
    {
        qDebug() << Q_FUNC_INFO << "TODO";
    }
}
