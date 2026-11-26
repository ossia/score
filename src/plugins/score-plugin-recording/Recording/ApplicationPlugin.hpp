#pragma once
#include "Session/RecordOptions.hpp"
#include "Takes/PlaceAddressTakes.hpp"

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <score_plugin_recording_export.h>

#include <QPointF>
#include <QTimer>

#include <optional>

#include <memory>
#include <vector>
namespace ossia::net
{
class parameter_base;
}
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
class IntervalModel;
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

  //! Starts a session on these addresses, recorded into `box` from now
  RecordSession* recordAddressesInto(
      Scenario::IntervalModel& box, const std::vector<State::Address>& addresses,
      AddressPlacement placement, std::optional<QPointF> nodePosition = {},
      std::optional<RecordOptions> options = {});
  //! The addresses selected in the explorer of a document
  std::vector<State::Address> selectedAddresses(const score::DocumentContext& ctx) const;
  //! Starts a session on these addresses, placed in a new box at `pt`.
  //! Without options: from the first value, playback starting from `pt`
  RecordSession* recordAddresses(
      Scenario::ProcessModel& scenario, Scenario::Point pt,
      const std::vector<State::Address>& addresses, AddressPlacement placement,
      std::optional<RecordOptions> options = {});
  //! What the Record button records in a document: the ports selected, the
  //! audio and MIDI outputs of the processes selected, else of those armed.
  //! `armed` says it is the latter.
  std::vector<const Process::Port*>
  portsToRecord(const score::DocumentContext& ctx, bool* armed = nullptr) const;

  //! Starts recording these ports of a document; one session at a time.
  //! Null when nothing could be recorded.
  RecordSession* startSession(
      const score::DocumentContext& ctx, const std::vector<const Process::Port*>& ports,
      RecordOptions options);
  //! Ends the session: its takes are placed
  void stopSession();
  //! Ends the session recording in `doc` now, its takes placed
  void endSessionOf(score::Document& doc);
  //! A session records in this document
  bool recording(const score::DocumentContext& ctx) const noexcept;
  RecordSession* session() const noexcept { return m_session.get(); }
  //! The options of a recording started without choosing them
  RecordOptions defaultOptions() const;

  void on_documentCloseRequested(score::Document& doc) override;
  void on_closeDocument(score::Document& doc) override;
  void on_documentSaveAs(score::Document& doc, const QString& newFileName) override;
  score::GUIElements makeGUIElements() override;

  //! The Record button: records the ports selected, or the audio and MIDI
  //! outlets of the processes selected; stops the recording when one runs
  void recordSelection();
  //! Places the last bars the armed processes played (Capture)
  void capture();
  //! Keeps what the armed processes of a document play, while it plays
  void startCapture(const score::Document& doc);
  void stopCapture();
  RecordSession* captureSession() const noexcept { return m_capture.get(); }
  //! The same, in a given document (remote control)
  void recordSelection(
      const score::DocumentContext& ctx, std::optional<QPointF> nodePosition = {},
      std::optional<RecordOptions> options = {});

  void on_createdDocument(score::Document& doc) override;
  void on_loadedDocument(score::Document& doc) override;

private:
  Engine::ApplicationPlugin* m_ossiaplug{};
  QAction* m_stopAction{};
  QAction* m_recordAct{};
  QTimer m_blink;
  QTimer m_captureWatch;

  //! score:/recording/active and /waiting of each document
  struct Published
  {
    const score::Document* document{};
    ossia::net::parameter_base* active{};
    ossia::net::parameter_base* waiting{};
    ossia::net::parameter_base* capturing{};
    ossia::net::parameter_base* message{};
  };
  std::vector<Published> m_published;
  void publish(score::Document& doc);
  void publishState();

  void showRecording(bool);
  //! Says at the pointer why nothing records
  void tell(const QString& why) const;
  //! What the last recording or capture had to say (lost data, a tempo
  //! change...), to the user and on score:/recording/message
  void report(const score::Document* doc, const QStringList& warnings);
  RecordSession* adopt(std::unique_ptr<RecordSession> session, const score::DocumentContext& ctx);

  std::unique_ptr<RecordSession> m_session;
  std::unique_ptr<RecordSession> m_capture;
  const score::Document* m_captureDocument{};
  //! Its armed processes could not be recorded in this playback
  const score::Document* m_captureBlocked{};
  QAction* m_captureAct{};
  void watchPlayback(score::Document& doc);
  const score::Document* m_sessionDocument{};
};
}
