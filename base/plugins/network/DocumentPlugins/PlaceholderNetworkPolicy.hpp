#pragma once
#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/Session.hpp>
class PlaceholderNetworkPolicy : public NetworkPluginPolicy
{
    public:
        template<typename Deserializer>
        PlaceholderNetworkPolicy(Deserializer&& vis, QObject* parent) :
            NetworkPluginPolicy{parent}
        {
            vis.writeTo(*this);
        }

        Session* session() const override
        { return m_session; }

        Session* m_session{};
};
