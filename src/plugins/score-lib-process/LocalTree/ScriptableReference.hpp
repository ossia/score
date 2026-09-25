#pragma once
#include <State/Address.hpp>
#include <State/Expression.hpp>

#include <Process/State/MessageNode.hpp>

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <score_lib_process_export.h>

#include <QPointer>

#include <verdigris>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace ossia::net
{
class device_base;
class node_base;
class parameter_base;
}

namespace LocalTree
{
struct ScriptableSnapshot;

//! Which state sent outside of a scenario run the values of published controls come from
enum class Recall : uint8_t
{
  None,
  //! The end state, sent at stop
  Stop,
  //! A state played while stopped
  State
};

//! Publishes the scriptable namespace and maps addresses to objects and back.
class SCORE_LIB_PROCESS_EXPORT ScriptableTreeBase : public score::DocumentPlugin
{
  W_OBJECT(ScriptableTreeBase)
public:
  using score::DocumentPlugin::DocumentPlugin;
  ~ScriptableTreeBase() override;

  virtual ossia::net::device_base& device() const noexcept = 0;
  //! Copy of the namespace for scripts on other threads; GUI thread only
  virtual std::shared_ptr<const ScriptableSnapshot> snapshot() const noexcept = 0;
  void snapshotChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, snapshotChanged)

  //! The object a published node stands for; member receives which of its parameters
  virtual QObject*
  objectAt(const ossia::net::node_base& node, QString& member) const noexcept = 0;
  virtual ossia::net::node_base*
  nodeOf(const QObject& object, const QString& member) const noexcept = 0;

  //! The states, ports, events and syncs holding an address anchored to the object
  virtual std::vector<QObject*> referrers(const QObject& target) const = 0;
  //! The objects the addresses of a state, port, event or sync are anchored to
  virtual std::vector<QObject*> targets(const QObject& referrer) const = 0;

  //! Re-derives the referrer's local addresses from their anchors and anchors
  //! the others by name; pasted copies publish under new names.
  virtual void follow(QObject& referrer) = 0;

  //! Parameter to send to for the address of a dynamic port not created yet;
  //! the port takes it over when it appears.
  virtual ossia::net::parameter_base*
  reserve(const ::State::Address& address, const ossia::value& sample) = 0;

  //! Objects the selection refers to or that refer to it; drawn emphasized.
  void setEmphasized(std::vector<const QObject*> objects);
  bool emphasized(const QObject& object) const noexcept;
  void emphasisChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, emphasisChanged)
  //! Emitted when a namespace node starts standing for the object
  void published(QObject* object) E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, published, object)
  //! The referrer's references may have broken or resolved; null means all. Coalesced.
  void referencesChanged(QObject* referrer)
      E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, referencesChanged, referrer)
  void executionStarted() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, executionStarted)
  //! Emitted when the execution has released the published controls
  void executionStopped() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, executionStopped)

  //! Whether the last run wrote control values that only the execution received
  virtual bool hasPlayedValues() const noexcept = 0;
  //! Stores these values in the document, as Record playback would
  virtual void keepPlayedValues() = 0;
  void playedValuesChanged() E_SIGNAL(SCORE_LIB_PROCESS_EXPORT, playedValuesChanged)

  //! Read from any thread
  Recall recall() const noexcept { return m_recall.load(std::memory_order_acquire); }
  //! Read from any thread
  uint32_t runsEnded() const noexcept
  {
    return m_runsEnded.load(std::memory_order_acquire);
  }
  //! A write from a run that has ended since counts as written at its stop
  Recall recallAt(Recall recall, uint32_t run) const noexcept
  {
    return (recall == Recall::None && run != runsEnded()) ? Recall::Stop : recall;
  }

protected:
  std::atomic<Recall> m_recall{Recall::None};
  std::atomic<uint32_t> m_runsEnded{};

private:
  std::vector<QPointer<const QObject>> m_emphasized;
};

//! Anchors a local-device address to the object published there; returns whether it has an anchor
SCORE_LIB_PROCESS_EXPORT bool anchor(::State::Address& a, const score::DocumentContext& ctx);
SCORE_LIB_PROCESS_EXPORT bool
anchor(::State::AddressAccessor& a, const score::DocumentContext& ctx);
//! Every address inside the expression
SCORE_LIB_PROCESS_EXPORT void anchor(::State::Expression& e, const score::DocumentContext& ctx);

//! Reconciles address and anchor: an anchor publishing elsewhere is dropped,
//! a dangling one yields to the object at the address, a bare address gets anchored.
SCORE_LIB_PROCESS_EXPORT void settle(::State::Address& a, const score::DocumentContext& ctx);
SCORE_LIB_PROCESS_EXPORT void
settle(::State::AddressAccessor& a, const score::DocumentContext& ctx);
//! Every address inside the expression
SCORE_LIB_PROCESS_EXPORT void settle(::State::Expression& e, const score::DocumentContext& ctx);
//! Every message of the tree; returns whether an anchor changed
SCORE_LIB_PROCESS_EXPORT bool
settle(Process::MessageNode& node, const score::DocumentContext& ctx);

//! The address the anchored object currently publishes; unset if it does not publish.
SCORE_LIB_PROCESS_EXPORT ::State::Address
derive(const ::State::Anchor& a, const score::DocumentContext& ctx);

//! member receives which of the object's parameters the address is
SCORE_LIB_PROCESS_EXPORT QObject* publishedObject(
    const ::State::Address& a, const score::DocumentContext& ctx, QString& member);

//! Whether something is published at this local-device address; nullopt
//! when the address is not on the local device.
SCORE_LIB_PROCESS_EXPORT std::optional<bool>
published(const ::State::Address& a, const score::DocumentContext& ctx);
//! Whether the document has what an address names. Patterns and addresses
//! of devices not shown in the explorer count as Found.
enum class AddressStatus
{
  Found,
  //! On the local device, but nothing is published there
  Unpublished,
  NoDevice,
  NoNode
};
SCORE_LIB_PROCESS_EXPORT AddressStatus
addressStatus(const ::State::Address& a, const score::DocumentContext& ctx);
//! What to tell the user about a status that is not Found
SCORE_LIB_PROCESS_EXPORT QString describe(AddressStatus s);
SCORE_LIB_PROCESS_EXPORT bool broken(const ::State::Address& a, const score::DocumentContext& ctx);
//! Whether the address is the state node of a published process, whose value
//! is an opaque blob not meant to be edited by hand
SCORE_LIB_PROCESS_EXPORT bool
isProcessState(const ::State::Address& a, const score::DocumentContext& ctx);

//! Replaces each anchor of the tree or expression with the mapping's result;
//! a null result removes the anchor
using AnchorMapping = std::function<std::shared_ptr<const ::State::Anchor>(
    const std::shared_ptr<const ::State::Anchor>&)>;
SCORE_LIB_PROCESS_EXPORT void mapAnchors(Process::MessageNode& n, const AnchorMapping& f);
SCORE_LIB_PROCESS_EXPORT void mapAnchors(::State::Expression& e, const AnchorMapping& f);

using AddressMapping
    = std::function<std::optional<::State::AddressAccessor>(const ::State::AddressAccessor&)>;
//! Replaces each address of the expression with the mapping's result, if any
SCORE_LIB_PROCESS_EXPORT void
mapAddresses(::State::Expression& e, const AddressMapping& map, bool& changed);

//! The address updated from its anchor; unchanged when it has no anchor or
//! the anchored object does not publish anymore.
SCORE_LIB_PROCESS_EXPORT ::State::Address
derived(const ::State::Address& a, const score::DocumentContext& ctx);
SCORE_LIB_PROCESS_EXPORT ::State::AddressAccessor
derived(const ::State::AddressAccessor& a, const score::DocumentContext& ctx);
}
