#pragma once
#include <Dataflow/PortItem.hpp>
#include <Process/Dataflow/Port.hpp>

namespace Gfx
{
class TextureInlet;
class TextureOutlet;
}
UUID_METADATA(
    ,
    Process::Port,
    Gfx::TextureInlet,
    "5ac86198-2d03-4830-9e41-a6d529922d29")
UUID_METADATA(
    ,
    Process::Port,
    Gfx::TextureOutlet,
    "f1c71046-b754-49a5-8e66-d01374773dfc")
namespace Gfx
{

class TextureInlet : public Process::Inlet
{
  W_OBJECT(TextureInlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(TextureInlet)
  TextureInlet() = delete;
  ~TextureInlet() override;
  TextureInlet(const TextureInlet&) = delete;
  TextureInlet(Id<Process::Port> c, QObject* parent);

  TextureInlet(DataStream::Deserializer& vis, QObject* parent);
  TextureInlet(JSONObject::Deserializer& vis, QObject* parent);
  TextureInlet(DataStream::Deserializer&& vis, QObject* parent);
  TextureInlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override { return Process::PortType::Texture; }
};

class TextureOutlet : public Process::Outlet
{
  W_OBJECT(TextureOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(TextureOutlet)
  TextureOutlet() = delete;
  ~TextureOutlet() override;
  TextureOutlet(const TextureOutlet&) = delete;
  TextureOutlet(Id<Process::Port> c, QObject* parent);

  TextureOutlet(DataStream::Deserializer& vis, QObject* parent);
  TextureOutlet(JSONObject::Deserializer& vis, QObject* parent);
  TextureOutlet(DataStream::Deserializer&& vis, QObject* parent);
  TextureOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override { return Process::PortType::Texture; }
};

struct TextureInletFactory final : public Dataflow::AutomatablePortFactory
{
  using Model_T = TextureInlet;
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};

struct TextureOutletFactory final : public Dataflow::AutomatablePortFactory
{
  using Model_T = TextureOutlet;
  UuidKey<Process::Port> concreteKey() const noexcept override
  {
    return Metadata<ConcreteKey_k, Model_T>::get();
  }

  Model_T* load(const VisitorVariant& vis, QObject* parent) override
  {
    return score::deserialize_dyn(vis, [&](auto&& deserializer) {
      return new Model_T{deserializer, parent};
    });
  }
};
}
