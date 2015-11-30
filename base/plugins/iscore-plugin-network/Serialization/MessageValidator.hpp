#pragma once
class MessageMapper;
class Session;
struct NetworkMessage;

class MessageValidator
{
    public:
        MessageValidator(Session& s, MessageMapper& map);

        bool validate(NetworkMessage m);

    private:
        Session& m_session;
        MessageMapper& m_mapper;
};
