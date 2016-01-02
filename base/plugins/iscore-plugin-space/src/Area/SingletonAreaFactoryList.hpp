#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <src/Area/AreaFactoryList.hpp>
#include <iscore_plugin_space_export.h>

namespace Space
{
class ISCORE_PLUGIN_SPACE_EXPORT SingletonAreaFactoryList final :
        public iscore::FactoryListInterface
{
           ISCORE_FACTORY_LIST_DECL(AreaFactory)

    public:
        // TODO generalize this.
        template<typename Key>
        auto get(const Key& k) const
        {
            return list().get(k);
        }

        const auto& get() const
        {
            return list().get();
        }

};

}
