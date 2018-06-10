// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_curve.hpp"

#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>
#include <Curve/Segment/Noise/NoiseSegment.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Curve/Segment/Sin/SinSegment.hpp>
#include <Curve/Settings/CurveSettingsFactory.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score_plugin_curve_commands_files.hpp>

score_plugin_curve::score_plugin_curve() : QObject{}
{
  qRegisterMetaType<Curve::Settings::Mode>();
  qRegisterMetaTypeStreamOperators<Curve::Settings::Mode>();
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_curve::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& factoryName) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Curve::SegmentFactory, Curve::SegmentFactory_T<Curve::LinearSegment>,
         Curve::SegmentFactory_T<Curve::PowerSegment>,
         Curve::SegmentFactory_T<Curve::NoiseSegment>,
         Curve::SegmentFactory_T<Curve::PointArraySegment>,
         Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Sin>>,
         Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Square>>,
         Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Triangle>>,
         Curve::SegmentFactory_T<Curve::PeriodicSegment<Curve::Saw>>

         >,
      FW<score::SettingsDelegateFactory, Curve::Settings::Factory>>(
      ctx, factoryName);
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_curve::factoryFamilies()
{
  return make_ptr_vector<score::InterfaceListBase, Curve::SegmentList>();
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_curve::make_commands()
{
  using namespace Curve;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Curve::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
    #include <score_plugin_curve_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
