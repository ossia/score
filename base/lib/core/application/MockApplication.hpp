#pragma once
#include <core/application/ApplicationInterface.hpp>

namespace iscore
{
namespace testing
{

struct MockApplication final :
        public iscore::ApplicationInterface
{
public:
    MockApplication()
    {
        m_instance = this;
    }

    const iscore::ApplicationContext& context() const override
    {
        throw;
    }

    const iscore::ApplicationComponents& components() const override
    {
        static iscore::ApplicationComponentsData d0;
        static iscore::ApplicationComponents d1{d0};
        return d1;
    }
};

}
}
