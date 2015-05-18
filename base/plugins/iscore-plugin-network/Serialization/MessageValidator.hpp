#pragma once
#include "MessageMapper.hpp"
#include "NetworkMessage.hpp"
class Session;

class MessageValidator
{
    public:
        MessageValidator(Session& s, MessageMapper& map);

        bool validate(NetworkMessage m);

    private:
        Session& m_session;
        MessageMapper& m_mapper;
};
