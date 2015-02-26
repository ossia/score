#pragma once
#include <tools/NamedObject.hpp>

namespace iscore
{
    class PanelPresenterInterface;
    class PanelModelInterface : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~PanelModelInterface() = default;
    };
}
