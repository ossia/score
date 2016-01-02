#pragma once
#include "BaseProperty.hpp"

class ISCORE_PLUGIN_OSSIA_EXPORT BaseCallbackWrapper : public BaseProperty
{
    public:
        using BaseProperty::BaseProperty;
        ~BaseCallbackWrapper()
        {
            addr->removeCallback(callbackIt);
        }

        OSSIA::Address::iterator callbackIt{};
};
