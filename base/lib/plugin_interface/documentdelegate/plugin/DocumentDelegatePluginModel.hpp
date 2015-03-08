#pragma once
#include <public_interface/tools/NamedObject.hpp>

namespace iscore
{
    class DocumentDelegatePluginModel : public NamedObject
    {
            Q_OBJECT
        public:
            using NamedObject::NamedObject;
            virtual ~DocumentDelegatePluginModel() = default;
    };
}
