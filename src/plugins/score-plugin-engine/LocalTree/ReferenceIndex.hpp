#pragma once
#include <State/Address.hpp>

#include <LocalTree/ScriptableReference.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <score_plugin_engine_export.h>

#include <verdigris>

#include <functional>
#include <optional>
#include <vector>

namespace score
{
struct DocumentContext;
}
namespace LocalTree
{
class DocumentPlugin;

//! Two-way index between addresses held by states, ports, conditions and
//! triggers and the objects they resolve to. A reference outliving its target
//! keeps the last address.
class SCORE_PLUGIN_ENGINE_EXPORT ReferenceIndex final : public QObject
{
  W_OBJECT(ReferenceIndex)
public:
  explicit ReferenceIndex(DocumentPlugin& tree);
  ~ReferenceIndex();

  //! Tracked until destroyed
  void add(QObject& referrer);

  std::vector<QObject*> referrers(const QObject& target) const;
  std::vector<QObject*> targets(const QObject& referrer) const;
  struct Broken
  {
    ::State::Address address;
    QObject* referrer{};
    AddressStatus status{};
  };
  //! Referrers holding an address that resolves to nothing
  std::vector<Broken> broken() const;
  //! Call once the document has a device explorer
  void watchExplorer();

  void refresh(const QObject& target);
  //! Re-anchors the dangling references to a newly published address
  void recover(const ::State::Address& published);
  void unpublished(const QObject* target);

  void changed() E_SIGNAL(SCORE_PLUGIN_ENGINE_EXPORT, changed)

public:
  using Mapping = std::function<std::optional<::State::AddressAccessor>(
      const ::State::AddressAccessor&)>;
  //! Not undoable
  void rewrite(QObject& referrer, const Mapping& map);

private:
  void update(QObject& referrer);
  void remove(QObject* referrer);
  //! Schedules changed() and referencesChanged; null for every referrer
  void notify(QObject* referrer);
  //! Schedules changed() alone
  void schedule();
  void unlink(const QObject* target, QObject* referrer);
  void unlinkAddress(const ::State::Address& address, QObject* referrer);
  void unlinkDevice(const QString& device, QObject* referrer);
  //! Throttled
  void explorerChanged(const QString& device);
  //! Also notifies the referrers of devices the explorer does not have
  void explorerDevicesChanged();
  //! If so the anchor is stale and the name takes precedence
  bool namesOther(const ::State::Address& a, const QObject* target) const;

  struct Ref
  {
    ::State::Address address;
    QPointer<QObject> target;
    //! Current or last target; never dereferenced, so usable after it is gone
    const QObject* key{};
    bool anchored{};
    bool local{};
    //! The anchor and the name resolve to different objects
    bool misanchored{};
    bool dangling() const noexcept { return local && !target; }
  };
  struct Entry
  {
    std::vector<Ref> refs;
  };

  DocumentPlugin& m_tree;
  ossia::hash_map<QObject*, Entry> m_entries;
  ossia::hash_map<const QObject*, std::vector<QObject*>> m_byTarget;
  ossia::hash_set<const QObject*> m_watched;
  ossia::hash_map<::State::Address, std::vector<QObject*>, std::hash<::State::Address>>
      m_byAddress;
  ossia::hash_map<QString, std::vector<QObject*>> m_byDevice;
  ossia::hash_set<QString> m_changedDevices;
  bool m_allDevicesChanged{};
  bool m_missingDevicesChanged{};
  QTimer m_explorerChanges;
  ossia::hash_set<QObject*> m_pending;
  bool m_pendingAll{};
  bool m_scheduled{};
};
}
