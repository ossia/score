#pragma once
#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/ClientSession.hpp>
class NetworkDocumentClientPlugin : public NetworkPluginPolicy
{
    public:
        NetworkDocumentClientPlugin(ClientSession* s, iscore::Document* doc);

        ClientSession* session() const
        { return m_session; }

    private:
        ClientSession* m_session{};
        iscore::Document* m_document{};
};


