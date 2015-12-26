#pragma once
#include "BaseProperty.hpp"

class BaseCallbackWrapper : public BaseProperty
{
    public:
        using BaseProperty::BaseProperty;
        ~BaseCallbackWrapper()
        {
            addr->removeCallback(m_callbackIt);
        }

    protected:
        OSSIA::Address::iterator m_callbackIt{};
};
