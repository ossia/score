#pragma once
#include <State/Address.hpp>

#include <Process/ProcessComponent.hpp>

#include <LocalTree/ScriptableReference.hpp>


#include <score/model/Component.hpp>
#include <score/model/EntityMap.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/parameter.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QMetaObject>

#include <score_lib_process_export.h>

#include <nano_observer.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace score
{
class ModelMetadata;
}
namespace Process
{
class Port;
class ControlInlet;
}

namespace LocalTree
{
class ScriptableTreeBase;
//! What a value written into a published control does
enum class ControlWrites
{
  //! Undoable command
  Edit,
  //! Sent to the execution only, displayed until stop
  Execution,
  //! The whole run becomes one command at stop
  Record,
  //! Ignored, e.g. the end state sent at stop
  Drop
};

struct ScriptableRoots
{
  ossia::net::node_base& controls;
  ossia::net::node_base& triggers;
  ossia::net::node_base& conditions;

  //! The execution may still hold the parameter, so the device owner frees the node.
  std::function<void(ossia::net::node_base&)> retire;

  //! Hides a node while its address still resolves
  std::function<void(ossia::net::node_base&)> hide;

  //! Restores a node retired during the current run, keeping the execution's parameter valid
  std::function<ossia::net::node_base*(ossia::net::node_base&, const std::string&)> revive;

  std::function<void(ossia::net::node_base&, const QObject&, const QString&)> tag;

  //! An object that may hold namespace addresses; registering twice is harmless
  std::function<void(QObject&)> referrer;

  //! Notifies when playback ends
  ScriptableTreeBase* tree{};

  //! Edit when unset
  std::function<ControlWrites(Recall)> writes;

  //! Called with the previous value before a control changes while recording
  std::function<void(Process::ControlInlet&, const ossia::value&)> recorded;

  //! Called when a value reached the execution only
  std::function<void(Process::ControlInlet&, const ossia::value&)> played;
};

inline void announceReferrer(const ScriptableRoots& roots, QObject& object)
{
  if(roots.referrer)
    roots.referrer(object);
}

inline void tagNode(
    const ScriptableRoots& roots, ossia::net::node_base& node, const QObject& object,
    const QString& member = {})
{
  if(roots.tag)
    roots.tag(node, object, member);
}

inline void retireNode(const ScriptableRoots& roots, ossia::net::node_base& node)
{
  if(roots.retire)
    roots.retire(node);
  else if(auto parent = node.get_parent())
    parent->remove_child(node);
}

//! Does not remove the node, so that it can be retired
struct ParameterBinding
{
  ossia::net::parameter_base& param;
  std::optional<ossia::net::parameter_base::callback_index> callback;
  QMetaObject::Connection connection;
  //! Checked by lambdas still queued on the event loop
  std::shared_ptr<bool> alive = std::make_shared<bool>(true);

  explicit ParameterBinding(ossia::net::parameter_base& p)
      : param{p}
  {
  }
  ParameterBinding(const ParameterBinding&) = delete;
  ParameterBinding& operator=(const ParameterBinding&) = delete;
  //! Applies a value as if the parameter had received it
  virtual void write(const ossia::value&) { }
  virtual ~ParameterBinding()
  {
    *alive = false;
    QObject::disconnect(connection);
    if(callback)
      param.remove_callback(*callback);
  }
};

SCORE_LIB_PROCESS_EXPORT ::State::Address addressOfNode(const ossia::net::node_base& n);

//! Name given to an object whose own name was taken; restored when it is
//! unpublished while still carrying it.
struct GivenName
{
  QString name;
  std::function<void()> restore;
  bool active{};

  //! Called when the object is renamed or published
  void follow(const QString& current) { active = restore && current == name; }
  //! Called when the object is unpublished but not destroyed
  void operator()()
  {
    if(std::exchange(active, false))
      restore();
  }
};

SCORE_LIB_PROCESS_EXPORT
void giveName(
    GivenName& given, score::ModelMetadata& metadata, const QString& own,
    const QString& name);
SCORE_LIB_PROCESS_EXPORT
void giveName(GivenName& given, Process::Port& port, const QString& name);

//! writeBack gets the name the device picked when it differs, e.g. on collision
template <typename F>
ossia::net::node_base* createScriptableNode(
    const ScriptableRoots& roots, ossia::net::node_base& parent, const QString& name,
    F&& writeBack)
{
  ossia::net::node_base* node{};
  if(roots.revive)
    node = roots.revive(parent, name.toStdString());
  if(!node)
    node = parent.create_child(name.toStdString());
  if(!node)
    return nullptr;
  if(auto real = QString::fromStdString(node->get_name()); real != name)
    writeBack(real);
  return node;
}

template <typename F>
void renameScriptableNode(ossia::net::node_base& node, const QString& name, F&& writeBack)
{
  const auto wanted = name.toStdString();
  if(node.get_name() == wanted)
    return;
  node.set_name(wanted);
  if(auto real = QString::fromStdString(node.get_name()); real != name)
    writeBack(real);
}

//! Publishes a process under score:/controls/<name>; the node exists only
//! while something is published.
class SCORE_LIB_PROCESS_EXPORT ScriptableProcessBase
    : public Process::GenericProcessComponent<const score::DocumentContext>
{
  ABSTRACT_COMPONENT_METADATA(
      ScriptableProcessBase, "ffd963c1-07f3-445f-b914-d36f59a08edf")
public:
  static constexpr bool is_unique = true;
  ScriptableProcessBase(
      ScriptableRoots roots, Process::ProcessModel& proc,
      const score::DocumentContext& ctx, QObject* parent);
  ~ScriptableProcessBase() override;

  ossia::net::node_base* node() const noexcept { return m_node; }
  ossia::net::node_base* node(const Process::Port& port) const noexcept;
  ossia::net::node_base* stateNode() const noexcept;

  //! For dynamic ports: created hidden until a port of that name appears
  ossia::net::parameter_base* reserve(const std::string& name, const ossia::value& sample);

private:
  struct PortEntry;
  struct Kept;

  //! Hides the node of a removed port until a port of the same name takes it
  //! back, with its own value if withValue, else the last one written to it
  void keep(ossia::net::node_base& node, bool withValue);
  ossia::net::node_base* takeKept(const QString& name, std::optional<ossia::value>& written);
  void dropKept(const QString& name);

  void refresh();
  void renameNode();
  //! restoreNames when the process is not being destroyed
  void removeNode(bool restoreNames);
  void publish(PortEntry& e);
  void unpublish(PortEntry& e, bool restoreName);
  void syncState();

protected:
  ScriptableRoots m_roots;

private:
  ossia::net::node_base* m_node{};
  GivenName m_given;
  std::unique_ptr<ParameterBinding> m_state;
  std::vector<std::unique_ptr<PortEntry>> m_ports;
  ossia::hash_map<const Process::Port*, PortEntry*> m_byPort;
  std::vector<std::unique_ptr<Kept>> m_kept;
};

class SCORE_LIB_PROCESS_EXPORT ScriptableProcessComponent final
    : public ScriptableProcessBase
{
  COMPONENT_METADATA("ed8196b3-c432-4405-a89b-ffc98a1e8f08")
public:
  using ScriptableProcessBase::ScriptableProcessBase;
};

//! For processes containing other processes; others use ScriptableProcessComponent
class SCORE_LIB_PROCESS_EXPORT ScriptableProcessFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ScriptableProcessFactory, "a6313dd7-00a4-4c65-afb7-3552f0c84216")
public:
  ~ScriptableProcessFactory() override;
  virtual bool matches(const Process::ProcessModel&) const noexcept = 0;
  virtual ScriptableProcessBase* make(
      ScriptableRoots roots, Process::ProcessModel& proc,
      const score::DocumentContext& ctx, QObject* parent) const = 0;
};

class SCORE_LIB_PROCESS_EXPORT ScriptableProcessFactoryList final
    : public score::InterfaceList<ScriptableProcessFactory>
{
public:
  ~ScriptableProcessFactoryList();
};

SCORE_LIB_PROCESS_EXPORT ScriptableProcessBase* makeScriptableProcess(
    ScriptableRoots roots, Process::ProcessModel& proc, const score::DocumentContext& ctx,
    QObject* parent);

class SCORE_LIB_PROCESS_EXPORT ScriptableProcessGroup final
    : public ScriptableProcessBase
    , public Nano::Observer
{
  COMPONENT_METADATA("9b9f1041-3cda-46ea-b94b-aa0d113cea83")
public:
  ScriptableProcessGroup(
      ScriptableRoots roots, Process::ProcessModel& proc,
      score::EntityMap<Process::ProcessModel>& children,
      const score::DocumentContext& ctx, QObject* parent);
  ~ScriptableProcessGroup();

private:
  void add(Process::ProcessModel& proc);
  void remove(const Process::ProcessModel& proc);

  std::vector<std::pair<Process::ProcessModel*, ScriptableProcessBase*>> m_children;
};

//! Copy of the namespace for scripts on other threads, rebuilt on the GUI thread
struct SCORE_LIB_PROCESS_EXPORT ScriptableSnapshot
{
  struct Entry
  {
    QString kind; //!< "node", "impulse" or "value"
    QString address;
    QStringList children;
  };
  //! Keyed by "<root>" for a root and "<root>/<path>" below it
  ossia::hash_map<QString, Entry> entries;

  static std::shared_ptr<const ScriptableSnapshot> build(const ScriptableRoots& roots);
};

//! score:/controls/<process>[/<port>|/preset]; empty when not published.
SCORE_LIB_PROCESS_EXPORT ::State::Address scriptableAddress(const Process::ProcessModel&);
SCORE_LIB_PROCESS_EXPORT ::State::Address scriptableAddress(const Process::Port&);
//! Controls of a scriptable process are published whether or not flagged
SCORE_LIB_PROCESS_EXPORT bool publishedWithProcess(const Process::Port&) noexcept;
SCORE_LIB_PROCESS_EXPORT bool wantsPublished(const Process::Port&) noexcept;
SCORE_LIB_PROCESS_EXPORT ::State::Address
scriptableStateAddress(const Process::ProcessModel&);
}
