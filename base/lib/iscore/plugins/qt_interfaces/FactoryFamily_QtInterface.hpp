#pragma once
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace iscore
{

    // Reimplement in plug-in if the plug-in offers a NEW ABSTRACT TYPE of data.
    // Example : the Inspector plug-in provides an interface for an inspector widget factory.
    class FactoryFamily_QtInterface
    {
        public:
            virtual ~FactoryFamily_QtInterface();
            virtual std::vector<FactoryFamily> factoryFamilies() = 0;
    };
}

#define FactoryFamily_QtInterface_iid "org.ossia.i-score.plugins.FactoryFamily_QtInterface"

Q_DECLARE_INTERFACE(iscore::FactoryFamily_QtInterface, FactoryFamily_QtInterface_iid)
