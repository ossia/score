#pragma once
#include <iscore/tools/NamedObject.hpp>
class ProtocolFactoryInterface;
namespace iscore
{
class FactoryInterface;
}


class ProtocolList
{
    public:
        ProtocolList() = default;

        ProtocolList(ProtocolList&&) = delete;
        ProtocolList(const ProtocolList&) = delete;
        ProtocolList& operator=(const ProtocolList&) = delete;
        ProtocolList& operator=(ProtocolList&&) = delete;

        ProtocolFactoryInterface* protocol(const QString&) const;
        void registerFactory(iscore::FactoryInterface*);
        QStringList protocols() const;

    private:
        std::vector<ProtocolFactoryInterface*> m_protocols;
};
