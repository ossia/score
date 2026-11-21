#pragma once
#include <Process/Execution/ProcessComponent.hpp>

#include <Media/Metro/MetroModel.hpp>

#include <ossia/dataflow/node_process.hpp>

#include <nano_observer.hpp>

#include <QString>

#include <score_plugin_media_export.h>

#include <memory>
#include <optional>

namespace Execution
{

/**
 * @brief Runs a metronome.
 *
 * The sounds that come from files reach the node from here, decoded: those of
 * the model, and those of the paths the node's file inlets receive while it
 * runs, from a cable or an address. Such a path replaces the file of the model
 * for this execution only, until the control is edited again: the document is
 * not changed.
 */
class SCORE_PLUGIN_MEDIA_EXPORT MetroComponent final
    : public ::Execution::ProcessComponent_T<Media::Metro::Model, ossia::node_process>
{
  COMPONENT_METADATA("f36a8077-b60a-42f2-8250-b581d335fd17")
public:
  MetroComponent(
      Media::Metro::Model& element, const ::Execution::Context& ctx, QObject* parent);

  ~MetroComponent();

  //! Files whose decoding was started for a path received while running.
  int executionDecodes() const noexcept { return m_decodes; }

private:
  //! Hands the sounds that come from files to the node.
  void updateSounds();

  //! A path came in on a file inlet of the node. UI thread.
  void on_executionPath(int file, QString path);
  void decodePending();

  //! The edited control wins over what came in while running.
  void on_modelPath(int file, const ossia::value& path);

  struct Decoded : Nano::Observer
  {
    explicit Decoded(MetroComponent& self)
        : self{self}
    {
    }
    MetroComponent& self;
    void changed() { self.updateSounds(); }
  };

  //! What an inlet received while running, in place of the model's file.
  struct ExecutionFile
  {
    bool active{};
    std::optional<QString> pending;
    std::shared_ptr<Media::AudioFile> file;
  };

  Decoded m_decoded{*this};
  ExecutionFile m_files[2];
  bool m_scheduled{};
  int m_decodes{};
};

using MetroComponentFactory = ::Execution::ProcessComponentFactory_T<MetroComponent>;
}
