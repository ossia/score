#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Threedim/GeometryViewer/Metadata.hpp>

namespace Gfx::GeometryViewer
{
//! How the geometry is drawn
enum class Mode
{
  Shaded,
  Wireframe,
  Points,
  VertexColors,
  TextureCoordinates
};

//! What is behind the geometry
enum class Background
{
  None,
  Black,
  Dark,
  Grey,
  White,
  Gradient
};

/**
 * Renders a geometry with the Model Display renderer, from a camera orbiting
 * a centre, and shows the result in its node.
 */
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::GeometryViewer::Model)
  W_OBJECT(Model)

public:
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  ~Model() override;

private:
  void init();
  QString prettyName() const noexcept override;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::GeometryViewer::Model>;
}
