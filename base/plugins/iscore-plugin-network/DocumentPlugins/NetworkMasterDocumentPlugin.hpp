#pragma once
#include <Repartition/session/MasterSession.hpp>

#include "NetworkDocumentPlugin.hpp"

namespace iscore {
struct DocumentContext;
}  // namespace iscore

namespace iscore
{
}
class MasterNetworkPolicy : public iscore_plugin_networkPolicy
{
    public:
        MasterNetworkPolicy(MasterSession* s,
                            iscore::DocumentContext& c);

        MasterSession* session() const override
        { return m_session; }

    private:
        MasterSession* m_session{};
};
