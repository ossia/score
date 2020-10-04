#include "score_plugin_jit.hpp"

#include <Process/Execution/ProcessComponent.hpp>
#include <score_plugin_engine.hpp>
#include <score_plugin_library.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <JitCpp/ApplicationPlugin.hpp>
#include <JitCpp/JitModel.hpp>
#include <Bytebeat/Bytebeat.hpp>
#include <Texgen/Texgen.hpp>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/ManagedStatic.h>
#include <llvm/Support/Signals.h>
#include <llvm/Support/TargetSelect.h>
#include <score_plugin_jit_commands_files.hpp>
#if defined(SCORE_JIT_HAS_TEXGEN)
#include <score_plugin_gfx.hpp>
#endif
score_plugin_jit::score_plugin_jit()
{
  using namespace llvm;
  sys::PrintStackTraceOnErrorSignal({});

  atexit(llvm_shutdown);
  InitializeNativeTarget();
  InitializeNativeTargetAsmPrinter();
  InitializeNativeTargetAsmParser();
}

score_plugin_jit::~score_plugin_jit() {}

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_jit::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory
      , Jit::JitEffectFactory
      , Jit::BytebeatEffectFactory
    #if defined(SCORE_JIT_HAS_TEXGEN)
      , Jit::TexgenEffectFactory
    #endif
      >,
      FW<Process::LayerFactory
      , Jit::LayerFactory
      , Jit::BytebeatLayerFactory
    #if defined(SCORE_JIT_HAS_TEXGEN)
      , Jit::TexgenLayerFactory
    #endif
      >,
      FW<Execution::ProcessComponentFactory
      , Execution::JitEffectComponentFactory
      , Jit::BytebeatExecutorFactory
    #if defined(SCORE_JIT_HAS_TEXGEN)
      , Jit::TexgenExecutorFactory
    #endif
      >
      >(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_jit::make_commands()
{
  using namespace Jit;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Jit::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_jit_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
score::GUIApplicationPlugin* score_plugin_jit::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Jit::ApplicationPlugin{app};
}

std::vector<score::PluginKey> score_plugin_jit::required() const
{
  return {score_plugin_engine::static_key()
    #if defined(SCORE_JIT_HAS_TEXGEN)
        , score_plugin_gfx::static_key()
    #endif
        , score_plugin_library::static_key()};
}

#include <score/plugins/PluginInstances.hpp>

SCORE_EXPORT_PLUGIN(score_plugin_jit)
