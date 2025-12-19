#pragma once
#include <Process/LayerView.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>

#include <Effect/EffectLayer.hpp>

#include <score/graphics/RectItem.hpp>

namespace Process
{
template <typename Model_T>
class EffectProcessFactory_T final : public Process::ProcessModelFactory
{
public:
  virtual ~EffectProcessFactory_T() = default;

  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }
  QString prettyName() const noexcept override
  {
    return Metadata<PrettyName_k, Model_T>::get();
  }
  QString category() const noexcept override
  {
    return Metadata<Category_k, Model_T>::get();
  }

  Descriptor descriptor(QString) const noexcept override;
  Descriptor descriptor(const Process::ProcessModel&) const noexcept override;

  ProcessFlags flags() const noexcept override
  {
    return Metadata<ProcessFlags_k, Model_T>::get();
  }

  QString customConstructionData() const noexcept override;

  Model_T* make(
      const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
      const score::DocumentContext& ctx, QObject* parent) override
  {
    return new Model_T{duration, data, id, parent};
  }

  Model_T* load(
      const VisitorVariant& vis, const score::DocumentContext& ctx,
      QObject* parent) final override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};

template <typename Model_T>
QString EffectProcessFactory_T<Model_T>::customConstructionData() const noexcept
{
  static_assert(std::is_same<Model_T, void>::value, "can't be used like this");
  return {};
}

class SCORE_LIB_PROCESS_EXPORT EffectLayerFactory_Base : public Process::LayerFactory
{
public:
  EffectLayerFactory_Base();
  ~EffectLayerFactory_Base();

  LayerView* makeLayerView(
      const Process::ProcessModel&, const Process::Context& context,
      QGraphicsItem* parent) const final override;

  LayerPresenter* makeLayerPresenter(
      const Process::ProcessModel& lm, Process::LayerView* v,
      const Process::Context& context, QObject* parent) const final override;

  score::ResizeableItem* makeItem(
      const Process::ProcessModel& proc, const Process::Context& ctx,
      QGraphicsItem* parent) const override;
};

template <typename Model_T,typename ExtView_T = void, typename ScriptView_T = void>
class EffectLayerFactory_T final : public EffectLayerFactory_Base
{
public:
  virtual ~EffectLayerFactory_T() = default;

private:
  UuidKey<Process::ProcessModel> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  QWidget* makeScriptUI(
      Process::ProcessModel& proc, const score::DocumentContext& ctx,
      QWidget* parent) const final override
  {
    (void)parent;
    try
    {
      if constexpr(!std::is_same_v<ScriptView_T, void>)
        return new ScriptView_T{safe_cast<Model_T&>(proc), ctx, parent};
    }
    catch(...)
    {
    }
    return nullptr;
  }


  bool hasExternalUI(
      const Process::ProcessModel& proc,
      const score::DocumentContext& ctx) const noexcept override
  {
    if constexpr(requires { ((Model_T&)proc).hasExternalUI(); })
      return ((Model_T&)proc).hasExternalUI();
    return false;
  }

  QWidget* makeExternalUI(
      Process::ProcessModel& proc, const score::DocumentContext& ctx,
      QWidget* parent) const final override
  {
    (void)parent;
    try
    {
      if constexpr(!std::is_same_v<ExtView_T, void>)
        return new ExtView_T{safe_cast<Model_T&>(proc), ctx, parent};
    }
    catch(...)
    {
    }
    return nullptr;
  }

  bool matches(const UuidKey<Process::ProcessModel>& p) const override
  {
    return p == Metadata<ConcreteKey_k, Model_T>::get();
  }
};

template <typename Model_T, typename ScriptView_T>
using ScriptLayerFactory_T = EffectLayerFactory_T<Model_T, void, ScriptView_T>;
}
