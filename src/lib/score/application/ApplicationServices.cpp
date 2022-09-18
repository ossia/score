#include "ApplicationServices.hpp"

namespace score
{

ApplicationServices& AppServices() noexcept
{
  static ApplicationServices services;
  return services;
}

}
