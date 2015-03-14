#include "StateWidget.hpp"
#include <State/State.hpp>
#include <State/Message.hpp>
#include <QGridLayout>
#include "MessageWidget.hpp"
StateWidget::StateWidget(const State& state, QWidget* parent):
    QWidget{parent}
{
    auto lay = new QVBoxLayout{this};
    // TODO : makeStateWidget(state)
    // state must have a way (State::m_name) to identify its kind
    if(state.data().canConvert<Message>())
    {
        lay->addWidget(new MessageWidget {state.data().value<Message>(), this});
    }
    else if(state.data().canConvert<MessageList>())
    {
        for(const Message& mess : state.data().value<MessageList>())
        {
            lay->addWidget(new MessageWidget {mess, this});
        }
    }
}
