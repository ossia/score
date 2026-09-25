#pragma once

#include <LocalTree/Device/LocalDevice.hpp>
#include <LocalTree/ReferenceIndex.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>
#include <LocalTree/ScriptableReference.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>

#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QPointer>

#include <atomic>
#include <memory>

#include <score_plugin_engine_export.h>
namespace score
{
class ModelMetadata;
}

namespace Scenario
{
class ProcessModel;
class IntervalModel;
class EventModel;
class TimeSyncModel;
class StateModel;
}

namespace ossia::net
{
class node_base;
class parameter_base;
}
namespace Execution
{
class DocumentPlugin;
}
namespace score
{
struct DocumentContext;
}
namespace LocalTree
{
class DocumentPlugin;

//! Scope of a state played from the UI: an undoable edit while stopped.
struct SCORE_PLUGIN_ENGINE_EXPORT StateRecall
{
  StateRecall(const score::DocumentContext& ctx, const Execution::DocumentPlugin& exec);
  ~StateRecall();
  StateRecall(const StateRecall&) = delete;
  StateRecall& operator=(const StateRecall&) = delete;

private:
  DocumentPlugin* m_tree{};
};

class Interval;
class ScriptableInterval;
struct ScriptableSnapshot;
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final : public ScriptableTreeBase
{
public:
  DocumentPlugin(const score::DocumentContext& doc, QObject* parent);

  ~DocumentPlugin();

  void init();

  void on_documentClosing() override;
  ossia::net::device_base& device() const noexcept override { return *m_localDevice; }

  Protocols::LocalDevice& localDevice() { return m_localDeviceWrapper; }

  //! Snapshot of the scriptable namespace, safe to read from any thread.
  std::shared_ptr<const ScriptableSnapshot> snapshot() const noexcept override;

  const ReferenceIndex& references() const noexcept { return m_references; }
  ReferenceIndex& references() noexcept { return m_references; }

  std::vector<QObject*> referrers(const QObject& target) const override
  {
    return m_references.referrers(target);
  }
  std::vector<QObject*> targets(const QObject& referrer) const override
  {
    return m_references.targets(referrer);
  }
  ossia::net::parameter_base*
  reserve(const ::State::Address& address, const ossia::value& sample) override;
  void follow(QObject& referrer) override;
  bool hasPlayedValues() const noexcept override;
  void keepPlayedValues() override;

  QObject*
  objectAt(const ossia::net::node_base& node, QString& member) const noexcept override;
  ossia::net::node_base*
  nodeOf(const QObject& object, const QString& member) const noexcept override;

  void connectToStop();

private:
  void create();
  void cleanup();
  void createScriptable();
  void cleanupScriptable();
  void anchorDocument();
  void retire(ossia::net::node_base& node);
  ossia::net::node_base* revive(ossia::net::node_base& parent, const std::string& name);
  void flushRetired();
  ControlWrites controlWrites(Recall recall);
  void recorded(Process::ControlInlet& port, const ossia::value& before);
  void played(Process::ControlInlet& port, const ossia::value& value);
  //! Converts an address from a document older than the scriptable namespace
  std::optional<::State::AddressAccessor>
  migrated(const ::State::AddressAccessor& a) const;
  void commitRecorded(Recall recall);

public:
  //! Brackets a state sent to the published controls outside the run; the
  //! recall ends once its writes have been applied.
  void beginRecall(Recall recall);
  void endRecall(Recall recall);

private:
  void tag(ossia::net::node_base& node, const QObject& object, const QString& member);
  void onTreeChanged();
  void onNodeChanged(ossia::net::node_base&) { onTreeChanged(); }
  void onNodeRenamed(ossia::net::node_base&, std::string);
  void onAttributeChanged(ossia::net::node_base&, const std::string&) { onTreeChanged(); }
  void onParameterChanged(const ossia::net::parameter_base&) { onTreeChanged(); }
  void rebuildSnapshot();

  Interval* m_root{};
  ScriptableInterval* m_scriptable{};
  ossia::net::node_base* m_controls{};
  ossia::net::node_base* m_triggers{};
  ossia::net::node_base* m_conditions{};
  std::vector<std::pair<ossia::net::node_base*, std::string>> m_retired;
  bool m_flushConnected{};
  bool m_stopConnected{};
  std::vector<std::pair<QPointer<Process::ControlInlet>, ossia::value>> m_recorded;
  //! Latest value of each control played since the run started
  std::vector<std::pair<QPointer<Process::ControlInlet>, ossia::value>> m_played;

  void untag(const ossia::net::node_base& node);

  struct Tagged
  {
    QPointer<QObject> object;
    //! Still usable for erasing after the object is destroyed
    const QObject* key{};
    QString member;
  };
  ossia::hash_map<const ossia::net::node_base*, Tagged> m_byNode;
  ossia::hash_map<const QObject*, ossia::small_vector<std::pair<QString, ossia::net::node_base*>, 2>>
      m_byObject;
  std::shared_ptr<const ScriptableSnapshot> m_snapshot;
  bool m_snapshotScheduled{};
  ReferenceIndex m_references{*this};
  std::unique_ptr<ossia::net::device_base> m_localDevice;
  Protocols::LocalDevice m_localDeviceWrapper;
};
}
