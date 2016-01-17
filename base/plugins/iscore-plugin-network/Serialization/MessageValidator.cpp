#include <QList>

#include "../Repartition/session/Session.hpp"
#include "MessageValidator.hpp"
#include "Serialization/MessageMapper.hpp"
#include "Serialization/NetworkMessage.hpp"
#include <iscore/tools/SettableIdentifier.hpp>

namespace Network
{
MessageValidator::MessageValidator(Session& s, MessageMapper& map):
    m_session{s},
    m_mapper{map}
{

}

bool MessageValidator::validate(NetworkMessage m)
{
    return  m_mapper.addresses().contains(m.address)
            && m_session.id().val().get() == m.sessionId;
}
}
