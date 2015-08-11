#pragma once
#include <iscore/tools/NamedObject.hpp>
class AreaFactory;
namespace iscore
{
class FactoryInterface;
}


class AreaFactoryList
{
    public:
        AreaFactoryList() = default;

        AreaFactoryList(AreaFactoryList&&) = delete;
        AreaFactoryList(const AreaFactoryList&) = delete;
        AreaFactoryList& operator=(const AreaFactoryList&) = delete;
        AreaFactoryList& operator=(AreaFactoryList&&) = delete;

        AreaFactory* factory(int type) const;
        AreaFactory* factory(const QString&) const;
        void registerFactory(iscore::FactoryInterface*);

        std::vector<AreaFactory*> factories() const;

    private:
        std::vector<AreaFactory*> m_factories;
};
