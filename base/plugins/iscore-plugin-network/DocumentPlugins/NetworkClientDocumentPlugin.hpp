#pragma once
#include <Repartition/session/ClientSession.hpp>

#include "NetworkDocumentPlugin.hpp"

namespace iscore {
class Document;
}  // namespace iscore

// MOVEME
class ClientNetworkPolicy : public iscore_plugin_networkPolicy
{
    public:
        ClientNetworkPolicy(ClientSession* s, iscore::Document* doc);

        ClientSession* session() const
        { return m_session; }

    private:
        ClientSession* m_session{};
        iscore::Document* m_document{};
};


