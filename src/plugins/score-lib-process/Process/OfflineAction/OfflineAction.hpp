#pragma once
#include <score/plugins/InterfaceList.hpp>

#include <ossia/detail/small_vector.hpp>

#include <score_lib_process_export.h>

#include <unordered_map>
namespace score
{
class Command;
class Document;
struct DocumentContext;
}
namespace Process
{
class ProcessModel;

/**
 * @brief Interface for applying an offline editing action to a process.
 *
 * There can be two kinds of actions:
 * - Simple actions which are triggered directly when clicking a button on a menu.
 *   For instance: "double gain", "humanize", ...
 * - Advanced actions which will open a custom modal dialog to allow the user to input the action
 *   settings.
 */
class SCORE_LIB_PROCESS_EXPORT OfflineAction : public score::InterfaceBase
{
  SCORE_INTERFACE(OfflineAction, "0d9e9135-bf4d-4ef2-b872-21be740f990c")
public:
  ~OfflineAction() override;

  virtual QString title() const noexcept = 0;
  virtual UuidKey<Process::ProcessModel> target() const noexcept = 0;
  virtual void apply(Process::ProcessModel& proc, const score::DocumentContext&) = 0;
};

class SCORE_LIB_PROCESS_EXPORT OfflineActionList final
    : public score::InterfaceListBase
    , public score::IndirectContainer<OfflineAction>
{
public:
  using OfflineActions = ossia::small_pod_vector<OfflineAction*, 4>;
  using factory_type = OfflineAction;
  using key_type = typename OfflineAction::ConcreteKey;
  using vector_type = score::IndirectContainer<OfflineAction>;

  OfflineActionList();
  ~OfflineActionList();

  OfflineActions
  actionsForProcess(const UuidKey<Process::ProcessModel>& key) const noexcept;

  static const MSVC_BUGGY_CONSTEXPR score::InterfaceKey static_interfaceKey() noexcept
  {
    return OfflineAction::static_interfaceKey();
  }

  score::InterfaceKey interfaceKey() const noexcept final override
  {
    return OfflineAction::static_interfaceKey();
  }

  void insert(std::unique_ptr<score::InterfaceBase> e) final override
  {
    if(auto result = dynamic_cast<factory_type*>(e.get()))
    {
      e.release();
      std::unique_ptr<factory_type> pf{result};
      vector_type::push_back(pf.get());
      actionsMap[result->target()].push_back(pf.get());

      auto k = pf->concreteKey();
      auto it = this->map.find(k);
      if(it == this->map.end())
      {
        this->map.emplace(std::make_pair(k, std::move(pf)));
      }
      else
      {
        score::debug_types(it->second.get(), result);
        it->second = std::move(pf);
      }

      added(vector_type::back());
    }
  }

  //! Get a particular factory from its ConcreteKey
  OfflineAction* get(const key_type& k) const noexcept
  {
    auto it = this->map.find(k);
    return (it != this->map.end()) ? it->second.get() : nullptr;
  }

  mutable Nano::Signal<void(const factory_type&)> added;

protected:
  ossia::hash_map<
      typename OfflineAction::ConcreteKey, std::unique_ptr<OfflineAction>>
      map;

  std::unordered_map<UuidKey<Process::ProcessModel>, OfflineActions> actionsMap;

  void optimize() noexcept final override
  {
    // score::optimize_hash_map(this->map);
    this->map.max_load_factor(0.1f);
    this->map.reserve(map.size());
  }

  OfflineActionList(const OfflineActionList&) = delete;
  OfflineActionList(OfflineActionList&&) = delete;
  OfflineActionList& operator=(const OfflineActionList&) = delete;
  OfflineActionList& operator=(OfflineActionList&&) = delete;
};
}
