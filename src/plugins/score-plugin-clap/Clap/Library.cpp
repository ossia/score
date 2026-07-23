#include "Library.hpp"

#include <Library/ProcessesItemModel.hpp>

#include <QDebug>
#include <algorithm>

namespace Clap
{

void LibraryHandler::setup(
    Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
{
  constexpr static const auto key = Metadata<ConcreteKey_k, Model>::get();

  QModelIndex node = model.find(key);
  if(node == QModelIndex{})
  {
    return;
  }
  auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());
  parent.key = {};

  auto& plug = ctx.guiApplicationPlugin<Clap::ApplicationPlugin>();

  // Build the plugin subtree into a detached node, grouping by category.
  auto build = [this, &plug](Library::ProcessNode& target) {
    QMap<QString, Library::ProcessNode*> categories;

    for(const auto& plugin : plug.plugins())
    {
      if(!plugin.valid)
        continue;
      QString category = getClapCategory(plugin.features);

      // Create category if it doesn't exist
      if(!categories.contains(category))
      {
        auto& cat_node = target.emplace_back(
            Library::ProcessData{{{}, category, {}}, {}}, &target);
        categories[category] = &cat_node;
      }

      QString pluginIdentifier = QString("%1:::%2").arg(plugin.path, plugin.id);

      Library::ProcessData pdata{{key, plugin.name, pluginIdentifier}, {}};
      Library::addToLibrary(*categories[category], std::move(pdata));
    }
  };

  // Rebuild the whole subtree using proper row insertion/removal instead of a
  // model reset: this keeps the view's selection/expansion state and does not
  // invalidate every unrelated QModelIndex. The subtree is built off to the
  // side and moved in under a single begin/endInsertRows.
  auto rebuild = [&model, node, &parent, build] {
    if(parent.childCount() > 0)
    {
      model.beginRemoveRows(node, 0, parent.childCount() - 1);
      parent.resize(0);
      model.endRemoveRows();
    }

    Library::ProcessNode built;
    build(built);

    if(const int n = built.childCount(); n > 0)
    {
      model.beginInsertRows(node, 0, n - 1);
      built.moveChildren(parent);
      model.endInsertRows();
    }
  };

  rebuild();

  con(plug, &Clap::ApplicationPlugin::pluginsChanged, this, rebuild);
}

QString LibraryHandler::getClapCategory(const QList<QString>& features) const
{
  // Map CLAP features to score categories
  // Based on CLAP feature IDs from clap/plugin-features.h

  for(const QString& feature : features)
  {
    // Instruments
    if(feature == "instrument" || feature == "synthesizer" || feature == "sampler"
       || feature == "drum" || feature == "drum-machine")
    {
      return "Instruments";
    }

    // Audio Effects
    if(feature == "audio-effect" || feature == "reverb" || feature == "delay" 
       || feature == "distortion" || feature == "dynamics" || feature == "compressor"
       || feature == "gate" || feature == "limiter" || feature == "transient-shaper"
       || feature == "eq" || feature == "filter" || feature == "flanger" 
       || feature == "chorus" || feature == "phaser" || feature == "tremolo"
       || feature == "vibrato" || feature == "pitch-shifter" || feature == "detuner"
       || feature == "mastering" || feature == "spatial")
    {
      return "Audio Effects";
    }

    // Generators
    if(feature == "oscillator" || feature == "noise-generator")
    {
      return "Generators";
    }

    // Analyzers
    if(feature == "analyzer" || feature == "meter" || feature == "tuner" 
       || feature == "spectrum-analyzer")
    {
      return "Analyzers";
    }

    // Utilities
    if(feature == "utility" || feature == "mixing" || feature == "channel-strip"
       || feature == "amplifier" || feature == "waveshaper")
    {
      return "Utilities";
    }

    // MIDI effects
    if(feature == "note-effect" || feature == "arpeggiator" || feature == "sequencer")
    {
      return "MIDI Effects";
    }
  }

  // Default category for uncategorized plugins
  return "Other";
}

}
