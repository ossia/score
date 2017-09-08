#pragma once
#include <core/application/ApplicationInterface.hpp>

namespace score
{
namespace testing
{

/**
 * @brief Application class for tests
 *
 * Used to setup a small application context for unit & integration tests where
 * serialization is required without needing the data from other plug-ins.
 */
struct MockApplication final : public score::ApplicationInterface
{
public:
  MockApplication()
  {
    m_instance = this;
  }

  const score::ApplicationContext& context() const override
  {
    throw;
  }

  const score::ApplicationComponents& components() const override
  {
    static score::ApplicationComponentsData d0;
    static score::ApplicationComponents d1{d0};
    return d1;
  }
};
}
}
