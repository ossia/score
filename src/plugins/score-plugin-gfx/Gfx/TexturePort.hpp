#pragma once
#include <Process/Dataflow/Port.hpp>

#include <Dataflow/PortItem.hpp>

#include <score_plugin_gfx_export.h>

namespace Gfx
{
class TextureInlet;
class TextureOutlet;
}
UUID_METADATA(, Process::Port, Gfx::TextureInlet, "5ac86198-2d03-4830-9e41-a6d529922d29")
UUID_METADATA(
    , Process::Port, Gfx::TextureOutlet, "f1c71046-b754-49a5-8e66-d01374773dfc")
namespace Gfx
{

class SCORE_PLUGIN_GFX_EXPORT TextureInlet : public Process::Inlet
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

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override
  {
    return Process::PortType::Texture;
  }
};

class SCORE_PLUGIN_GFX_EXPORT TextureOutlet : public Process::Outlet
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

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override
  {
    return Process::PortType::Texture;
  }

  int nodeId{-1};
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

  void setupInletInspector(
      const Process::Inlet& port, const score::DocumentContext& ctx, QWidget* parent,
      Inspector::Layout& lay, QObject* context) override;
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

  void setupOutletInspector(
      const Process::Outlet& port, const score::DocumentContext& ctx, QWidget* parent,
      Inspector::Layout& lay, QObject* context) override;
};
}

namespace Gfx
{
class GeometryInlet;
class GeometryOutlet;
}
UUID_METADATA(
    , Process::Port, Gfx::GeometryInlet, "f2ab26ea-415d-45a2-bfbc-2968c7c92a33")
UUID_METADATA(
    , Process::Port, Gfx::GeometryOutlet, "848061c5-e8a0-4a13-9985-e8df30ce6d4f")
namespace Gfx
{

class SCORE_PLUGIN_GFX_EXPORT GeometryInlet : public Process::Inlet
{
  W_OBJECT(GeometryInlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(GeometryInlet)
  GeometryInlet() = delete;
  ~GeometryInlet() override;
  GeometryInlet(const GeometryInlet&) = delete;
  GeometryInlet(Id<Process::Port> c, QObject* parent);

  GeometryInlet(DataStream::Deserializer& vis, QObject* parent);
  GeometryInlet(JSONObject::Deserializer& vis, QObject* parent);
  GeometryInlet(DataStream::Deserializer&& vis, QObject* parent);
  GeometryInlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override
  {
    return Process::PortType::Geometry;
  }
};

class SCORE_PLUGIN_GFX_EXPORT GeometryOutlet : public Process::Outlet
{
  W_OBJECT(GeometryOutlet)

  SCORE_SERIALIZE_FRIENDS
public:
  MODEL_METADATA_IMPL_HPP(GeometryOutlet)
  GeometryOutlet() = delete;
  ~GeometryOutlet() override;
  GeometryOutlet(const GeometryOutlet&) = delete;
  GeometryOutlet(Id<Process::Port> c, QObject* parent);

  GeometryOutlet(DataStream::Deserializer& vis, QObject* parent);
  GeometryOutlet(JSONObject::Deserializer& vis, QObject* parent);
  GeometryOutlet(DataStream::Deserializer&& vis, QObject* parent);
  GeometryOutlet(JSONObject::Deserializer&& vis, QObject* parent);

  VIRTUAL_CONSTEXPR Process::PortType type() const noexcept override
  {
    return Process::PortType::Geometry;
  }
};

struct GeometryInletFactory final : public Dataflow::AutomatablePortFactory
{
  using Model_T = GeometryInlet;
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

struct GeometryOutletFactory final : public Dataflow::AutomatablePortFactory
{
  using Model_T = GeometryOutlet;
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
