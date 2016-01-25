#pragma once
#include <boost/uuid/uuid.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
class ISCORE_LIB_BASE_EXPORT SerializableInterface
{
    public:
        virtual ~SerializableInterface();
        virtual boost::uuids::uuid uuid() = 0;
        virtual void serialize(const VisitorVariant& vis) const = 0;
};
}
