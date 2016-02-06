#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/tools/ForEachType.hpp>
#include <iscore/tools/std/Pointer.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/iterator/indirect_iterator.hpp>

#include <iscore_lib_base_export.h>
#include <QMetaType>

namespace bmi = boost::multi_index;

namespace iscore
{

/**
     * @brief The FactoryFamily class
     *
     * Keeps the factories, so that they can be found easily.
     */
class ISCORE_LIB_BASE_EXPORT FactoryListInterface
{
    public:
        static constexpr bool factory_list_tag = true;
        FactoryListInterface() = default;
        FactoryListInterface(const FactoryListInterface&) = delete;
        FactoryListInterface& operator=(const FactoryListInterface&) = delete;
        virtual ~FactoryListInterface();

        // Example : InspectorWidgetFactory
        virtual iscore::AbstractFactoryKey abstractFactoryKey() const = 0;

        // This function is called whenever a new factory interface
        // is added to this family.
        virtual void insert(std::unique_ptr<iscore::FactoryInterfaceBase>) = 0;
};


template<typename Map_T>
class IndirectMap
{
    public:
        auto begin()        { return boost::make_indirect_iterator(map.begin()); }
        auto begin() const  { return boost::make_indirect_iterator(map.begin()); }

        auto cbegin()       { return boost::make_indirect_iterator(map.cbegin()); }
        auto cbegin() const { return boost::make_indirect_iterator(map.cbegin()); }

        auto end()          { return boost::make_indirect_iterator(map.end()); }
        auto end() const    { return boost::make_indirect_iterator(map.end()); }

        auto cend()         { return boost::make_indirect_iterator(map.cend()); }
        auto cend() const   { return boost::make_indirect_iterator(map.cend()); }

        auto empty() const { return map.empty(); }

        template<typename K>
        auto find(K&& key)
        {
            return map.find(std::forward<K>(key));
        }

        template<typename E>
        auto insert(E&& elt)
        {
            return map.insert(std::forward<E>(elt));
        }

    protected:
        Map_T map;
};

// FIXME They should take an export macro also ?
template<typename FactoryType>
class ConcreteFactoryList :
        public iscore::FactoryListInterface,
        public IndirectMap<
                bmi::multi_index_container<
                    std::unique_ptr<FactoryType>,
                    bmi::indexed_by<
                        bmi::hashed_unique<
                            bmi::const_mem_fun<
                                iscore::GenericFactoryInterface<typename FactoryType::ConcreteFactoryKey>,
                                typename FactoryType::ConcreteFactoryKey,
                                &FactoryType::template key_value<typename FactoryType::ConcreteFactoryKey>
                            >
                        >
                    >
                >
        >
{
    public:
        using factory_type = FactoryType;
        using object_type = typename FactoryType::object_type;
        using key_type = typename FactoryType::ConcreteFactoryKey;

        virtual ~ConcreteFactoryList() noexcept
        {

        }

        static const iscore::AbstractFactoryKey& static_abstractFactoryKey() {
            return FactoryType::static_abstractFactoryKey();
        }

        iscore::AbstractFactoryKey abstractFactoryKey() const final override {
            return FactoryType::static_abstractFactoryKey();
        }

        void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override
        {
            if(auto pf = dynamic_unique_ptr_cast<FactoryType>(std::move(e)))
            {
                auto it = this->map.find(pf->template key<key_type>());
                if(it == this->map.end())
                {
                    this->map.insert(std::move(pf));
                }
            }
        }

        const auto& list() const
        { return this->map; }

        auto get(const key_type& k) const
        {
            auto it = this->map.find(k);
            return (it != this->map.end()) ? it->get() : nullptr;
        }
};
}

template<typename Base_T,
         typename... Args>
struct GenericFactoryInserter
{
        std::vector<std::unique_ptr<Base_T>> vec;
        GenericFactoryInserter()
        {
            vec.reserve(sizeof...(Args));
            for_each_type<TypeList<Args...>>(*this);
        }

        template<typename TheClass>
        void visit()
        {
            vec.push_back(std::make_unique<TheClass>());
        }
};

template<typename... Args>
auto make_ptr_vector()
{
    return GenericFactoryInserter<Args...>{}.vec;
}

template<typename Context_T,
         typename Factory_T>
struct FactoryBuilder // sorry padre for I have sinned
{
        static auto make(const Context_T&)
        {
            return std::make_unique<Factory_T>();
        }
};

template<typename Context_T,
         typename Base_T,
         typename... Args>
struct ContextualGenericFactoryInserter
{
        const Context_T& context;
        std::vector<std::unique_ptr<Base_T>> vec;
        ContextualGenericFactoryInserter(const Context_T& ctx):
            context{ctx}
        {
            vec.reserve(sizeof...(Args));
            for_each_type<TypeList<Args...>>(*this);
        }

        template<typename TheClass>
        void visit()
        {
            vec.push_back(FactoryBuilder<Context_T, TheClass>::make(context));
        }
};


template<typename Context_T, typename Base_T, typename... Args>
auto make_ptr_vector(const Context_T& context)
{
    return ContextualGenericFactoryInserter<Context_T, Base_T, Args...>{context}.vec;
}

#define ISCORE_STANDARD_FACTORY(FactorizedElementName) \
class FactorizedElementName ## FactoryTag {}; \
using FactorizedElementName ## FactoryKey = StringKey<FactorizedElementName ## FactoryTag>; \
Q_DECLARE_METATYPE(FactorizedElementName ## FactoryKey) \
\
class FactorizedElementName ## Factory : \
        public iscore::GenericFactoryInterface<FactorizedElementName ## FactoryKey> \
{ \
        ISCORE_ABSTRACT_FACTORY_DECL(#FactorizedElementName) \
    public: \
            using ConcreteFactoryKey = FactorizedElementName ## FactoryKey; \
        virtual std::unique_ptr<FactorizedElementName> make() = 0; \
}; \
class FactorizedElementName ## FactoryList final : public iscore::FactoryListInterface \
{ \
       ISCORE_FACTORY_LIST_DECL(FactorizedElementName ## Factory) \
};

