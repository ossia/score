#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
using ProtocolList = GenericFactoryMap_T<ProtocolFactory, ProtocolFactoryKey>;

class DynamicProtocolList : public iscore::FactoryListInterface
{
    public:
        iscore::FactoryBaseKey name() const override;
        void insert(iscore::FactoryInterfaceBase* e) override;

        const ProtocolList& list() const
        { return m_list; }

    private:
        ProtocolList m_list;
};
