#pragma once
#include <unordered_map>
#include <memory>
/*
concept FactoryList
{
    void inscribe(Factory*);
    Factory* get(Key&&);
    Container get();
}
*/

template<typename T, typename Key>
class GenericFactoryMap_T
{
    public:
        using key_t = Key;
        // TODO delete the factories in the end. Or unique_ptr ?
        void inscribe(std::unique_ptr<T> reg)
        {
            auto it = m_factories.find(reg->template key<Key>());
            if(it == m_factories.end())
            {
                m_factories.insert(std::make_pair(reg->template key<Key>(), std::move(reg))); // MULTI_INDEX....
            }
        }

        T* get(const Key& str) const
        {
            auto it = m_factories.find(str);
            return (it != m_factories.end()) ? it->second.get() : nullptr;
        }

        const auto& get() const
        {
            return m_factories;
        }

    private:
        std::unordered_map<Key, std::unique_ptr<T>> m_factories;
};
