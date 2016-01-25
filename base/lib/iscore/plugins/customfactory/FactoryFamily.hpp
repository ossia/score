#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/tools/ForEachType.hpp>
#include <iscore/tools/std/Pointer.hpp>
#include <iscore_lib_base_export.h>
#include <QMetaType>

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
        virtual iscore::FactoryBaseKey name() const = 0;

        // This function is called whenever a new factory interface
        // is added to this family.
        virtual void insert(std::unique_ptr<iscore::FactoryInterfaceBase>) = 0;
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



// FIXME They should take an export macro also ?

#define ISCORE_FACTORY_LIST_DECL(FactoryType) \
  private: \
     GenericFactoryMap_T<FactoryType, FactoryType::factory_key_type> m_list;\
  public: \
    static const iscore::FactoryBaseKey& staticFactoryKey() { \
        return FactoryType::staticFactoryKey(); \
    } \
    \
    iscore::FactoryBaseKey name() const final override { \
        return FactoryType::staticFactoryKey(); \
    } \
    void insert(std::unique_ptr<iscore::FactoryInterfaceBase> e) final override \
    { \
        if(auto pf = dynamic_unique_ptr_cast<FactoryType>(std::move(e))) \
            m_list.inscribe(std::move(pf)); \
    } \
    const auto& list() const \
    { return m_list; }\
    \
    using factory_type = FactoryType; \
    using object_type = typename FactoryType::object_type; \
  private:


#define ISCORE_STANDARD_FACTORY(FactorizedElementName) \
class FactorizedElementName ## FactoryTag {}; \
using FactorizedElementName ## FactoryKey = StringKey<FactorizedElementName ## FactoryTag>; \
Q_DECLARE_METATYPE(FactorizedElementName ## FactoryKey) \
\
class FactorizedElementName ## Factory : \
        public iscore::GenericFactoryInterface<FactorizedElementName ## FactoryKey> \
{ \
        ISCORE_FACTORY_DECL(#FactorizedElementName) \
    public: \
            using factory_key_type = FactorizedElementName ## FactoryKey; \
        virtual std::unique_ptr<FactorizedElementName> make() = 0; \
}; \
class FactorizedElementName ## FactoryList final : public iscore::FactoryListInterface \
{ \
       ISCORE_FACTORY_LIST_DECL(FactorizedElementName ## Factory) \
};

