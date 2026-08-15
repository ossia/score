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

  auto& plug = ctx.guiApplicationPlugin<Clap::ApplicationPlugin>();

  // Stage the whole plugin list grouped by category; the model owns tree
  // mutation and signals.
  auto make_plugs = [this, &plug] {
    QMap<QString, Library::StagedNode> categories;

    for(const auto& plugin : plug.plugins())
    {
      if(!plugin.valid)
        continue;
      QString category = getClapCategory(plugin.features);

      auto it = categories.find(category);
      if(it == categories.end())
        it = categories.insert(
            category, Library::StagedNode{{{{}, category, {}}, {}}, {}});

      QString pluginIdentifier = QString("%1:::%2").arg(plugin.path, plugin.id);
      it->children.push_back(
          Library::StagedNode{{{key, plugin.name, pluginIdentifier}, {}}, {}});
    }

    std::vector<Library::StagedNode> v;
    v.reserve(categories.size());
    for(auto& cat : categories) // QMap: already name-sorted
      v.push_back(std::move(cat));
    return v;
  };

  model.clearAnchorKey(key);
  model.replaceChildren(key, make_plugs());

  con(plug, &Clap::ApplicationPlugin::pluginsChanged, this,
      [model = QPointer{&model}, make_plugs] {
    if(model)
      model->replaceChildren(key, make_plugs());
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
