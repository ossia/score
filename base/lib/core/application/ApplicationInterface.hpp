#pragma once
#include <iscore_lib_base_export.h>
#include <iscore/application/ApplicationContext.hpp>
namespace iscore
{

class ISCORE_LIB_BASE_EXPORT ApplicationInterface
{
public:
    virtual ~ApplicationInterface();
    virtual const ApplicationContext& context() const = 0;
    static ApplicationInterface& instance();

    protected:
        static ApplicationInterface* m_instance;
};

}
