#include "score_plugin_faust.hpp"

#include <Library/LibraryInterface.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <Faust/EffectModel.hpp>
#include <Faust/Library.hpp>

#include <score_plugin_faust_commands_files.hpp>
#include <wobjectimpl.h>

#include <faust/dsp/llvm-dsp.h>

// Undefine macros defined by Qt / Verdigris
#if defined(__arm__)
#undef READ
#undef WRITE
#undef RESET
#undef OPTIONAL

#include <llvm/Support/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#endif

#include "Guitarix/tables.cpp"
#include "STK/tables.cpp"
template <typename T>
requires requires(T t)
{
  registerForeignFunction(t);
}
auto do_registerCustomForeignFunction(const T& str)
{
  registerForeignFunction(str);
}

template <typename T>
auto do_registerCustomForeignFunction(const T& str)
{
}
score_plugin_faust::score_plugin_faust()
{
#if defined(__arm__)
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmPrinters();
  llvm::InitializeAllAsmParsers();
#endif

  // Guitarix functions
  do_registerCustomForeignFunction("Ftube");
  do_registerCustomForeignFunction("Ranode");
  do_registerCustomForeignFunction("Ftrany");
  do_registerCustomForeignFunction("asymclip");
  do_registerCustomForeignFunction("asymclip2");
  do_registerCustomForeignFunction("asymclip3");
  do_registerCustomForeignFunction("opamp");
  do_registerCustomForeignFunction("opamp1");
  do_registerCustomForeignFunction("opamp2");
  do_registerCustomForeignFunction("asymhardclip");
  do_registerCustomForeignFunction("asymhardclip2");
  do_registerCustomForeignFunction("symclip");

  // STK functions
  do_registerCustomForeignFunction("loadPreset");
  do_registerCustomForeignFunction("loadPhonemeGains");
  do_registerCustomForeignFunction("loadPhonemeParameters");
  do_registerCustomForeignFunction("readMarmstk1");
  do_registerCustomForeignFunction("getValueDryTapAmpT60piano");
  do_registerCustomForeignFunction("getValueSustainPedalLevel");
  do_registerCustomForeignFunction("getValueLoudPole");
  do_registerCustomForeignFunction("getValuePoleValue");
  do_registerCustomForeignFunction("getValueLoudGain");
  do_registerCustomForeignFunction("getValueSoftGain");
  do_registerCustomForeignFunction("getValueDCBa1piano");
  do_registerCustomForeignFunction("getValuer1_1db");
  do_registerCustomForeignFunction("getValuer1_2db");
  do_registerCustomForeignFunction("getValuer2db");
  do_registerCustomForeignFunction("getValuer3db");
  do_registerCustomForeignFunction("getValueSecondStageAmpRatio");
  do_registerCustomForeignFunction("getValueSecondPartialFactor");
  do_registerCustomForeignFunction("getValueThirdPartialFactor");
  do_registerCustomForeignFunction("getValueBq4_gEarBalled");
  do_registerCustomForeignFunction("getValueStrikePosition");
  do_registerCustomForeignFunction("getValueEQBandWidthFactor");
  do_registerCustomForeignFunction("getValueEQGain");
  do_registerCustomForeignFunction("getValueDetuningHz");
  do_registerCustomForeignFunction("getValueSingleStringDecayRate");
  do_registerCustomForeignFunction("getValueSingleStringZero");
  do_registerCustomForeignFunction("getValueSingleStringPole");
  do_registerCustomForeignFunction("getValueStiffnessCoefficient");
  do_registerCustomForeignFunction("getValueReleaseLoopGainpiano");
  do_registerCustomForeignFunction("getValueLoopFilterb0piano");
  do_registerCustomForeignFunction("getValueLoopFilterb1piano");
  do_registerCustomForeignFunction("getValueLoopFilterb2piano");
  do_registerCustomForeignFunction("getValueLoopFiltera1piano");
  do_registerCustomForeignFunction("getValueLoopFiltera2piano");
  do_registerCustomForeignFunction("getValueBassLoopFilterb0piano");
  do_registerCustomForeignFunction("getValueBassLoopFilterb1piano");
  do_registerCustomForeignFunction("getValueBassLoopFiltera1piano");
  do_registerCustomForeignFunction("getValueDryTapAmpT60harpsichord");
  do_registerCustomForeignFunction("getValueReleaseLoopGainharpsichord");
  do_registerCustomForeignFunction("getValueLoopFilterb0harpsichord");
  do_registerCustomForeignFunction("getValueLoopFilterb1harpsichord");
  do_registerCustomForeignFunction("getValueLoopFilterb2harpsichord");
  do_registerCustomForeignFunction("getValueLoopFiltera1harpsichord");
  do_registerCustomForeignFunction("getValueLoopFiltera2harpsichord");
  do_registerCustomForeignFunction("getValueBassLoopFilterb0");
  do_registerCustomForeignFunction("getValueBassLoopFilterb1bass");
  do_registerCustomForeignFunction("getValueBassLoopFiltera1bass");
}

score_plugin_faust::~score_plugin_faust() { }

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_faust::make_commands()
{
  using namespace Faust;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_faust_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_faust::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Faust::FaustEffectFactory>,
      FW<Process::LayerFactory, Faust::LayerFactory>,
      FW<Library::LibraryInterface, Faust::LibraryHandler>,
      FW<Execution::ProcessComponentFactory, Execution::FaustEffectComponentFactory>,
      FW<Process::ProcessDropHandler, Faust::DropHandler>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_faust)
