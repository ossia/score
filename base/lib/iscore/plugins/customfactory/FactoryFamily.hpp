#pragma once
#include <QObject>
#include <functional>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>


namespace iscore
{
class FactoryInterfaceBase;

/**
     * @brief The FactoryFamily class
     *
     * Keeps the factories, so that they can be found easily.
     */
class FactoryListInterface
{
    public:
        virtual ~FactoryListInterface();

        // Example : InspectorWidgetFactory
        virtual iscore::FactoryBaseKey name() const = 0;

        // This function is called whenever a new factory interface
        // is added to this family.
        virtual void insert(iscore::FactoryInterfaceBase*) = 0;
};
}

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
    void insert(iscore::FactoryInterfaceBase* e) final override \
    { \
        if(auto pf = dynamic_cast<FactoryType*>(e)) \
            m_list.inscribe(pf); \
    } \
    const auto& list() const \
    { return m_list; }\
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

