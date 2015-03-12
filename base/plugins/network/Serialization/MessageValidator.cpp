#include "MessageValidator.hpp"
#include "../Repartition/session/Session.h"
MessageValidator::MessageValidator(Session& s, MessageMapper& map):
    m_session{s},
    m_mapper{map}
{

}

bool MessageValidator::validate(NetworkMessage m)
{
    return  m_mapper.addresses().contains(m.address)
            && m_session.id() == m.sessionId
            && m_session.localClient().id() == m.clientId;
}
