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

  auto reset_plugs = [this, &plug, &parent] {
    // Group plugins by category
    QMap<QString, Library::ProcessNode*> categories;

    for(const auto& plugin : plug.plugins())
    {
      if(!plugin.valid)
        continue;
      QString category = getClapCategory(plugin.features);

      // Create category if it doesn't exist
      if(!categories.contains(category))
      {
        auto& cat_node = parent.emplace_back(
            Library::ProcessData{{{}, category, {}}, {}}, &parent);
        categories[category] = &cat_node;
      }

      QString pluginIdentifier = QString("%1:::%2").arg(plugin.path, plugin.id);

      Library::ProcessData pdata{{key, plugin.name, pluginIdentifier}, {}};
      Library::addToLibrary(*categories[category], std::move(pdata));
    }
  };

  reset_plugs();

  con(plug, &Clap::ApplicationPlugin::pluginsChanged, this,
      [&plug, &model, node, &parent, reset_plugs] {
    if(parent.childCount() > 0)
    {
      model.beginRemoveRows(node, 0, parent.childCount() - 1);
      parent.resize(0);
      model.endRemoveRows();
    }

    int k = 0;
    for(const auto& clap : plug.plugins())
    {
      if(clap.valid)
        k++;
    }
    if(k > 0)
    {
      model.beginInsertRows(node, 0, k - 1);
      reset_plugs();
      model.endInsertRows();
    }
  });
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
