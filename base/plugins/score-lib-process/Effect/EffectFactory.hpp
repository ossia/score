#pragma once
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score_lib_process_export.h>

class QGraphicsItem;
namespace Control
{
struct RectItem;
}
namespace Process
{
class EffectModel;

/**
 * @brief The EffectFactory class
 *
 * An abstract factory for the generation of plug-ins.
 * This is meant to be subclassed by factories of
 * categories of plug-ins.
 * For instance : VSTEffectFactory, FaustEffectFactory, etc.
 *
 * For now a QString is passed but according to the needs
 * of various plug-in APIs this may change.
 *
 */
class SCORE_LIB_PROCESS_EXPORT EffectFactory :
        public score::Interface<EffectModel>
{
        SCORE_INTERFACE("3ffe0073-dfe0-4a7f-862f-220380ebcf08")
    public:
        ~EffectFactory() override;

        virtual QString prettyName() const = 0; // VST, FaUST, etc...
        virtual QString category() const = 0; // VST, FaUST, etc...

        /**
         * @brief makeM Creates an effect model
         * @param info Data used for the creation of the effect
         * @param parent Parent object
         * @return A valid effect instance if the info is correct, else nullptr.
         */
        virtual EffectModel* make(
                const QString& info, // plugin name ? faust code ? dll location ?
                const Id<EffectModel>&,
                QObject* parent) const = 0;

        /**
         * @brief load Loads an effect model
         * @param data Serialized data
         * @param parent Parent object
         * @return If the effect can be loaded, an instance,
         * else a MissingEffectModel with the serialized data should be returned.
         */
        virtual EffectModel* load(
                const VisitorVariant& data,
                QObject* parent) const = 0;
};

/**
 * @brief The GenericEffectFactory class
 *
 * Should handle most cases
 */
template<typename Model_T>
class EffectFactory_T final :
        public EffectFactory
{
    public:
        virtual ~EffectFactory_T() = default;

        static auto static_concreteKey()
        { return Metadata<ConcreteKey_k, Model_T>::get(); }
    private:
        UuidKey<Process::EffectModel> concreteKey() const noexcept override
        { return Metadata<ConcreteKey_k, Model_T>::get(); }

        QString prettyName() const override
        { return Metadata<PrettyName_k, Model_T>::get(); }
        QString category() const override
        { return Metadata<Category_k, Model_T>::get(); }

        Model_T* make(
                const QString& info, // plugin name ? faust code ? dll location ?
                const Id<EffectModel>& id,
                QObject* parent) const final override
        {
            return new Model_T{info, id, parent};
        }

        Model_T* load(
                const VisitorVariant& vis,
                QObject* parent) const final override
        {
            return score::deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new Model_T{deserializer, parent}; });
        }
};

/**
 * @brief The EffectFactoryList class
 *
 * If a factory cannot be found at load time
 * (for instance AU plug-in on Windows),
 * a MissingEffectModel should be returned.
 */
class SCORE_LIB_PROCESS_EXPORT EffectFactoryList final :
        public score::InterfaceList<EffectFactory>
{
    public:
        using object_type = Process::EffectModel;
        object_type* loadMissing(
                const VisitorVariant& vis,
                QObject* parent) const;
};

class SCORE_LIB_PROCESS_EXPORT EffectUIFactory
    : public score::Interface<EffectModel>
{
  SCORE_INTERFACE("0b57b4c4-7da5-4032-9ac7-6fac34896e10")
public:
  ~EffectUIFactory() override;

  virtual QGraphicsItem*
  makeItem(const Process::EffectModel& view, const score::DocumentContext& ctx, Control::RectItem* parent) const = 0;

  bool matches(const Process::EffectModel& p) const;
  virtual bool matches(const UuidKey<Process::EffectModel>&) const = 0;
};

template<typename Model_T, typename View_T>
class EffectUIFactory_T final :
    public EffectUIFactory
{
  public:
    virtual ~EffectUIFactory_T() = default;

  private:
    QGraphicsItem*
    makeItem(const Process::EffectModel& view, const score::DocumentContext& ctx, Control::RectItem* parent) const final override
    {
      return new View_T{safe_cast<const Model_T&>(view), ctx, parent};
    }

    bool matches(const UuidKey<Process::EffectModel>& p) const override
    {
      return p == Metadata<ConcreteKey_k, Model_T>::get();
    }

    UuidKey<EffectModel> concreteKey() const noexcept override
    {
      return Metadata<ConcreteKey_k, Model_T>::get();
    }
};

class SCORE_LIB_PROCESS_EXPORT EffectUIFactoryList final :
        public score::InterfaceList<EffectUIFactory>
{
  public:
    EffectUIFactory*
    findDefaultFactory(const EffectModel& proc) const;

    EffectUIFactory* findDefaultFactory(
        const UuidKey<EffectModel>& proc) const;
};
}
Q_DECLARE_METATYPE(UuidKey<Process::EffectFactory>)
