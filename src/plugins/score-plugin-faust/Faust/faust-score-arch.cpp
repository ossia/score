#include <Faust/FaustDSPWrapper.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>

#include <cmath>
#include <faust/dsp/dsp.h>
#include <faust/gui/GUI.h>
#include <faust/gui/JSONUI.h>
#include <faust/gui/meta.h>
#include <string.h>

#include <utility>
#include <vector>

/******************************************************************************
*******************************************************************************

                     VECTOR INTRINSICS

*******************************************************************************
*******************************************************************************/

<< includeIntrinsic >>

    /******************************************************************************
    *******************************************************************************

          ABSTRACT USER INTERFACE

    *******************************************************************************
    *******************************************************************************/

    //----------------------------------------------------------------------------
    //  FAUST generated signal processor
    //----------------------------------------------------------------------------

    << includeclass >>

    template <>
    struct Metadata<PrettyName_k, FaustDSP::Fx<mydsp>>
{
  static Q_DECL_RELAXED_CONSTEXPR const char* get() { return "==FAUST_NAME=="; }
};
template <>
struct Metadata<ObjectKey_k, FaustDSP::Fx<mydsp>>
{
  static Q_DECL_RELAXED_CONSTEXPR const char* get() { return "==FAUST_NAME=="; }
};
template <>
struct Metadata<ConcreteKey_k, FaustDSP::Fx<mydsp>>
{
  static Q_DECL_RELAXED_CONSTEXPR UuidKey<Process::ProcessModel> get() { return_uuid("==UUID=="); }
};

class score_faust_ == FAUST_NAME == final : public score::Plugin_QtInterface,
    public score::FactoryInterface_QtInterface
{
  SCORE_PLUGIN_METADATA(1, "==UUID==")

public:
  virtual ~score_faust_ == FAUST_NAME == ();

private:
  // Defined in FactoryInterface_QtInterface
  std::vector<std::unique_ptr<score::InterfaceBase>> factories(
      const score::ApplicationContext& ctx, const score::InterfaceKey& key) const override
  {
    return instantiate_factories<
        score::ApplicationContext,
        FW<Process::ProcessModelFactory, FaustDSP::ProcessFactory<mydsp>>,
        FW<Process::LayerFactory, FaustDSP::LayerFactory<mydsp>>,
        // FW<Process::InspectorWidgetDelegateFactory,
        // Shader::InspectorFactory>,
        FW<Execution::ProcessComponentFactory, FaustDSP::ExecutorFactory<mydsp>>>(ctx, key);
  }
};

score_faust_ == FAUST_NAME == ::~score_faust_ == FAUST_NAME == () { }
