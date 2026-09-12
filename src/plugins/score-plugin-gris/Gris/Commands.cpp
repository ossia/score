#include <Gris/Commands.hpp>

namespace Gris
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Gris"};
  return key;
}
} // namespace Gris
