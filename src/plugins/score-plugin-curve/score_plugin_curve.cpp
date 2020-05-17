// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_curve.hpp"

#include <Curve/ApplicationPlugin.hpp>
#include <Curve/Commands/CurveCommandFactory.hpp>
#include <Curve/Segment/CurveSegmentFactory.hpp>
#include <Curve/Segment/CurveSegmentList.hpp>
#include <Curve/Segment/EasingSegment.hpp>
#include <Curve/Segment/Linear/LinearSegment.hpp>

#include <Curve/Segment/PointArray/PointArraySegment.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Curve/Settings/CurveSettingsFactory.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <score_plugin_curve_commands_files.hpp>

score_plugin_curve::score_plugin_curve()
{
  qRegisterMetaType<Curve::Settings::Mode>();
  qRegisterMetaTypeStreamOperators<Curve::Settings::Mode>();
}

score_plugin_curve::~score_plugin_curve() = default;

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_curve::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& factoryName) const
{
  using namespace Curve;
  return instantiate_factories<
      score::ApplicationContext,
      FW<SegmentFactory,
         SegmentFactory_T<LinearSegment>,
         SegmentFactory_T<PowerSegment>,
         SegmentFactory_T<PointArraySegment>,
         SegmentFactory_T<Segment_backIn>,
         SegmentFactory_T<Segment_backOut>,
         SegmentFactory_T<Segment_backInOut>,
         SegmentFactory_T<Segment_bounceIn>,
         SegmentFactory_T<Segment_bounceOut>,
         SegmentFactory_T<Segment_bounceInOut>,
         SegmentFactory_T<Segment_quadraticIn>,
         SegmentFactory_T<Segment_quadraticOut>,
         SegmentFactory_T<Segment_quadraticInOut>,
         SegmentFactory_T<Segment_cubicIn>,
         SegmentFactory_T<Segment_cubicOut>,
         SegmentFactory_T<Segment_cubicInOut>,
         SegmentFactory_T<Segment_quarticIn>,
         SegmentFactory_T<Segment_quarticOut>,
         SegmentFactory_T<Segment_quarticInOut>,
         SegmentFactory_T<Segment_quinticIn>,
         SegmentFactory_T<Segment_quinticOut>,
         SegmentFactory_T<Segment_quinticInOut>,
         SegmentFactory_T<Segment_sineIn>,
         SegmentFactory_T<Segment_sineOut>,
         SegmentFactory_T<Segment_sineInOut>,
         SegmentFactory_T<Segment_circularIn>,
         SegmentFactory_T<Segment_circularOut>,
         SegmentFactory_T<Segment_circularInOut>,
         SegmentFactory_T<Segment_exponentialIn>,
         SegmentFactory_T<Segment_exponentialOut>,
         SegmentFactory_T<Segment_exponentialInOut>,
         SegmentFactory_T<Segment_elasticIn>,
         SegmentFactory_T<Segment_elasticOut>,
         SegmentFactory_T<Segment_elasticInOut>,
         SegmentFactory_T<Segment_perlinInOut>>,
      FW<score::SettingsDelegateFactory, Settings::Factory>>(ctx, factoryName);
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_curve::factoryFamilies()
{
  return make_ptr_vector<score::InterfaceListBase, Curve::SegmentList>();
}

score::GUIApplicationPlugin* score_plugin_curve::make_guiApplicationPlugin(
    const score::GUIApplicationContext& ctx)
{
  return new Curve::ApplicationPlugin{ctx};
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

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_curve)
