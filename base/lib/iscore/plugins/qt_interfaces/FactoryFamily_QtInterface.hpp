#pragma once

#include <vector>

namespace iscore {
class FactoryListInterface;
}  // namespace iscore

namespace iscore
{

    // Reimplement in plug-in if the plug-in offers a NEW ABSTRACT TYPE of data.
    // Example : the Inspector plug-in provides an interface for an inspector widget factory.
    class FactoryList_QtInterface
    {
        public:
            virtual ~FactoryList_QtInterface();
            virtual std::vector<FactoryListInterface*> factoryFamilies() = 0;
    };
}

#define FactoryList_QtInterface_iid "org.ossia.i-score.plugins.FactoryList_QtInterface"

Q_DECLARE_INTERFACE(iscore::FactoryList_QtInterface, FactoryList_QtInterface_iid)
