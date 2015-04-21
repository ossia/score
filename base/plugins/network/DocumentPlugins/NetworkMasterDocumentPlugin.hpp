#pragma once
#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/MasterSession.hpp>
class NetworkDocumentMasterPlugin : public NetworkPluginPolicy
{
    public:
        NetworkDocumentMasterPlugin(MasterSession* s, iscore::Document* doc);

        MasterSession* session() const override
        { return m_session; }

    private:
        MasterSession* m_session{};
        iscore::Document* m_document{};
};
