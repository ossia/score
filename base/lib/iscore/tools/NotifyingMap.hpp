#pragma once
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <nano_signal_slot.hpp>
#include <utility>
#include <array>
#include <iostream>

/**
 * @brief The NotifyingMap class
 *
 * This class is a wrapper over IdContainer.
 * Differences :
 *  - Deletes objects when they are removed ("ownership")
 *  - Sends signals after adding and before deleting.
 *
 * Tthe parent of the childs are the parents of the map.
 * Hence the objects shall not be deleted upon deletion of the map
 * itself, to prevent a double-free.
 *
 */
template<typename T>
class NotifyingMap
{
    public:
        // The real interface starts here
        using value_type = T;
        auto begin() const { return m_map.begin(); }
        auto cbegin() const { return m_map.cbegin(); }
        auto end() const { return m_map.end(); }
        auto cend() const { return m_map.cend(); }
        auto size() const { return m_map.size(); }
        bool empty() const { return m_map.empty(); }
        const auto& map() const { return m_map; }
        const auto& get() const { return m_map.get(); }
        T& at(const Id<T>& id) { return m_map.at(id); }
        T& at(const Id<T>& id) const { return m_map.at(id); }
        auto find(const Id<T>& id) const { return m_map.find(id); }

        // signals:
        mutable Nano::Signal<void(T&)> mutable_added;
        mutable Nano::Signal<void(const T&)> added;
        mutable Nano::Signal<void(const T&)> removing;
        mutable Nano::Signal<void(const T&)> removed;

        void add(T* t) {
            m_map.insert(t);

            mutable_added(*t);
            added(*t);
        }

        void remove(T* elt) {
            m_map.remove(elt->id());
            removed(*elt);
            delete elt;
        }

        void remove(const Id<T>& id) {
            auto elt = *m_map.get().find(id);

            removing(*elt);
            m_map.remove(id);
            removed(*elt);
            delete elt;
        }

    private:
        IdContainer<T> m_map;
};
