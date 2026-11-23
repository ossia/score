#pragma once
#include "Session/RecordOptions.hpp"
#include "Takes/PlaceAddressTakes.hpp"

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_recording_export.h>

#include <QTimer>

#include <memory>
#include <vector>
namespace Engine
{
class ApplicationPlugin;
}
class QAction;
namespace Process
{
class Port;
}
namespace score
{
struct DocumentContext;
class Document;
}
namespace Scenario
{
class ProcessModel;
struct Point;
} // namespace Scenario
namespace Recording
{
class RecordSession;
class SCORE_PLUGIN_RECORDING_EXPORT ApplicationPlugin final
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin() override;

  //! The addresses selected in the explorer, from `pt` of the scenario, as
  //! automations or as messages
  void record(Scenario::ProcessModel*, Scenario::Point pt);
  void recordMessages(Scenario::ProcessModel*, Scenario::Point pt);
  //! Starts a session on these addresses, placed in a new box at `pt`
  RecordSession* recordAddresses(
      Scenario::ProcessModel& scenario, Scenario::Point pt,
      const std::vector<State::Address>& addresses, AddressPlacement placement);

  //! Starts recording these ports of a document; one session at a time.
  //! Null when nothing could be recorded.
  RecordSession* startSession(
      const score::DocumentContext& ctx, const std::vector<const Process::Port*>& ports,
      RecordOptions options);
  //! Ends the session: its takes are placed
  void stopSession();
  //! A session records in this document
  bool recording(const score::DocumentContext& ctx) const noexcept;
  RecordSession* session() const noexcept { return m_session.get(); }
  //! The options of a recording started without choosing them
  RecordOptions defaultOptions() const;

  void on_closeDocument(score::Document& doc) override;
  score::GUIElements makeGUIElements() override;

  //! The Record button: records the ports selected, or the audio and MIDI
  //! outlets of the processes selected; stops the recording when one runs
  void recordSelection();

private:
  Engine::ApplicationPlugin* m_ossiaplug{};
  QAction* m_stopAction{};
  QAction* m_recordAct{};
  QTimer m_blink;

  void showRecording(bool);
  RecordSession* adopt(std::unique_ptr<RecordSession> session, const score::DocumentContext& ctx);

  std::unique_ptr<RecordSession> m_session;
  const score::Document* m_sessionDocument{};
};
}
