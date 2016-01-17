#pragma once
#include <Repartition/session/ClientSession.hpp>

#include "NetworkDocumentPlugin.hpp"

namespace iscore {
class Document;
}  // namespace iscore

// MOVEME
namespace Network
{
class ClientNetworkPolicy : public NetworkPolicyInterface
{
    public:
        ClientNetworkPolicy(ClientSession* s, iscore::Document* doc);

        ClientSession* session() const override
        { return m_session; }

    private:
        ClientSession* m_session{};
        iscore::Document* m_document{};
};
}
