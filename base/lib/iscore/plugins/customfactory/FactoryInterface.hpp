#pragma once
#include <unordered_map>
#include <string>

namespace iscore
{
// Base class for factories of elements whose type is not part of the base application.

class FactoryInterfaceBase
{
    public:
        virtual ~FactoryInterfaceBase();

        /**
         * @brief factoryName
         * @return The name of the general factory (ProtocolFactory, ProcessFactory)
         */
        virtual const std::string& factoryName() const = 0;
};

template<typename Key>
class FactoryKeyInterface
{
    public:
        virtual ~FactoryKeyInterface() = default;
        virtual const Key& key_impl() const = 0;
};
template <typename... Keys>
class GenericFactoryInterface_Base;

template<typename Key, typename... Keys>
class GenericFactoryInterface_Base<Key, Keys...> :
        public FactoryKeyInterface<Key>,
        public GenericFactoryInterface_Base<Keys...>
{
    public:
        virtual ~GenericFactoryInterface_Base() = default;
};

template<>
class GenericFactoryInterface_Base<> :
        public iscore::FactoryInterfaceBase
{
    public:
        virtual ~GenericFactoryInterface_Base() = default;
};

template<typename... Keys>
class GenericFactoryInterface : public GenericFactoryInterface_Base<Keys...>
{
    public:
        template<typename Key_T>
        const Key_T& key() const
        {
            return static_cast<const FactoryKeyInterface<Key_T>*>(this)->key_impl();
        }
};

}

#define ISCORE_FACTORY_DECL(Str) \
    public: \
    static const std::string& staticFactoryName() \
{ \
    static const std::string s{Str}; \
    return s; \
    } \
    \
    const std::string& factoryName() const final override \
{ return staticFactoryName(); } \
    private:

/*
concept FactoryList
{
    void inscribe(Factory*);
    Factory* get(Key&&);
    Container get();
}
*/

template<typename T, typename Key>
class GenericFactoryList_T
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


