#pragma once
#include <iscore/tools/NamedObject.hpp>

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
