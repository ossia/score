#pragma once
#include <Process/Process.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <Effect/EffectModel.hpp>
#include <boost/iterator/indirect_iterator.hpp>
namespace Media
{
namespace Effect
{
class ProcessModel;


template <typename T>
class EntityList
{
public:
  // The real interface starts here
  using value_type = T;
  auto begin() const
  {
    return boost::make_indirect_iterator(m_list.begin());
  }
  auto cbegin() const
  {
    return boost::make_indirect_iterator(m_list.cbegin());
  }
  auto end() const
  {
    return boost::make_indirect_iterator(m_list.end());
  }
  auto cend() const
  {
    return boost::make_indirect_iterator(m_list.cend());
  }
  auto size() const
  {
    return m_list.size();
  }
  bool empty() const
  {
    return m_list.empty();
  }
  auto& unsafe_list()
  {
    return m_list;
  }
  const auto& list() const
  {
    return m_list;
  }
  const auto& get() const
  {
    return m_list.get();
  }
  T& at(const Id<T>& id)
  {
    auto it = find(id);
    SCORE_ASSERT(it != m_list.end());
    return *it;
  }
  T& at(const Id<T>& id) const
  {
    auto it = find(id);
    SCORE_ASSERT(it != m_list.end());
    return **it;
  }
  T& at_pos(std::size_t n) const
  {
    SCORE_ASSERT(n < m_list.size());
    auto it = m_list.begin();
    std::advance(it, n);
    return **it;
  }
  auto find(const Id<T>& id) const
  {
    return ossia::find_if(m_list, [&] (auto ptr) { return ptr->id() == id; });
  }
  auto find(const Id<T>& id)
  {
    return ossia::find_if(m_list, [&] (auto ptr) { return ptr->id() == id; });
  }

  auto index(const Id<T>& id) const
  {
    auto it = ossia::find_if(m_list, [&] (auto ptr) { return ptr->id() == id; });;
    SCORE_ASSERT(it != m_list.end());
    return std::distance(m_list.begin(), it);
  }

  // signals:
  mutable Nano::Signal<void(T&)> mutable_added;
  mutable Nano::Signal<void(const T&)> added;
  mutable Nano::Signal<void(const T&)> removing;
  mutable Nano::Signal<void(const T&)> removed;
  mutable Nano::Signal<void()> orderChanged;

  void add(T* t)
  {
    SCORE_ASSERT(t);
    unsafe_list().push_back(t);

    mutable_added(*t);
    added(*t);
  }

  void erase(T& elt)
  {
    auto it = ossia::find(m_list, &elt);
    SCORE_ASSERT(it != m_list.end());

    removing(elt);
    m_list.erase(it);
    removed(elt);
  }

  void remove(T& elt)
  {
    erase(elt);
    delete &elt;
  }

  void remove(T* elt)
  {
    remove(*elt);
  }

  void remove(const Id<T>& id)
  {
    auto it = find(id);
    SCORE_ASSERT(it != m_list.end());
    auto& elt = **it;

    removing(elt);
    m_list.erase(it);
    removed(elt);
    delete &elt;
  }

  void clear()
  {
    while (!m_list.empty())
    {
      remove(*m_list.begin());
    }
  }

  void insert_at(std::size_t pos, T* t)
  {
    SCORE_ASSERT(pos <= m_list.size());
    auto it = m_list.begin();
    std::advance(it, pos);

    m_list.insert(it, t);
    mutable_added(*t);
    added(*t);
  }
  void move(const Id<T>& id, std::size_t pos)
  {
    auto it1 = find(id);
    SCORE_ASSERT(it1 != m_list.end());

    auto it2 = m_list.begin();
    SCORE_ASSERT(pos < m_list.size());
    std::advance(it2, pos);

    std::swap(it1, it2);
    orderChanged();
  }


private:
  std::list<T*> m_list;
};

/**
 * @brief The Media::Effect::ProcessModel class
 *
 * This class represents an effect chain.
 * Each effect should provide a component that will create
 * the corresponding LibMediaStream effect.
 *
 * Chaining two effects blocks [A] -> [B] is akin to
 * doing :
 *
 * MakeEffectSound(MakeEffectSound(Original sound, A, 0, 0), B, 0, 0)
 *
 */
class ProcessModel final : public Process::ProcessModel
{
        SCORE_SERIALIZE_FRIENDS
        PROCESS_METADATA_IMPL(Media::Effect::ProcessModel)

        Q_OBJECT
    public:
        explicit ProcessModel(
                const TimeVal& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent);


        ~ProcessModel() override;

        template<typename Impl>
        explicit ProcessModel(
                Impl& vis,
                QObject* parent) :
            Process::ProcessModel{vis, parent}
        {
            vis.writeTo(*this);
        }

        const EntityList<Process::EffectModel>& effects() const
        { return m_effects; }

        void insertEffect(Process::EffectModel* eff, int pos);
        void removeEffect(const Id<Process::EffectModel>&);
        void moveEffect(const Id<Process::EffectModel>&, int new_pos);

        int effectPosition(const Id<Process::EffectModel>& e) const;

        Process::Inlets inlets() const override
        {
          return {inlet.get()};
        }

        Process::Outlets outlets() const override
        {
          return {outlet.get()};
        }

        std::unique_ptr<Process::Inlet> inlet{};
        std::unique_ptr<Process::Outlet> outlet{};

    signals:
        void effectsChanged();

    private:
        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection& s) const override;
        // The actual effect instances
        EntityList<Process::EffectModel> m_effects;
};
}
}
