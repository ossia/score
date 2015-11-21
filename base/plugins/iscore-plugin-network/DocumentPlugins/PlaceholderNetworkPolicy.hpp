#pragma once
#include "NetworkDocumentPlugin.hpp"

#include <Repartition/session/Session.hpp>
class PlaceholderNetworkPolicy : public iscore_plugin_networkPolicy
{
    public:
        template<typename Deserializer>
        PlaceholderNetworkPolicy(Deserializer&& vis, QObject* parent) :
            iscore_plugin_networkPolicy{parent}
        {
            vis.writeTo(*this);
        }

        Session* session() const override
        { return m_session; }

        Session* m_session{};
};
