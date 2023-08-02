#include <Avnd/Factories.hpp>

#if __has_include(<kfr/dft/convolution.hpp>)
#include <kfr/kfr.h>
#if QT_VERSION_CHECK(KFR_VERSION_MAJOR, KFR_VERSION_MINOR, KFR_VERSION_PATCH) \
    >= QT_VERSION_CHECK(5, 0, 2)
#include <avnd/../../examples/Advanced/Utilities/Convolver.hpp>
#define AVND_HAS_CONVOLVER 1
#endif
#endif

namespace oscr
{
void instantiate_convolver(
    std::vector<score::InterfaceBase*>& fx,
    const score::ApplicationContext& ctx, const score::InterfaceKey& key)
{
#if AVND_HAS_CONVOLVER
  oscr::instantiate_fx<ao::Convolver>(fx, ctx, key);
#endif
}
}
