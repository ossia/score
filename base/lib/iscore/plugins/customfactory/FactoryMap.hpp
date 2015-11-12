#pragma once
#include <unordered_map>

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
        void inscribe(T* reg)
        {
            auto it = m_factories.find(reg->template key<Key>());
            if(it == m_factories.end())
            {
                m_factories.insert(std::make_pair(reg->template key<Key>(), reg)); // MULTI_INDEX....
            }
        }

        T* get(const Key& str) const
        {
            auto it = m_factories.find(str);
            return (it != m_factories.end()) ? it->second : nullptr;
        }

        const auto& get() const
        {
            return m_factories;
        }

    private:
        std::unordered_map<Key, T*> m_factories;
};
