#pragma once
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <QHBoxLayout>
#include <QOpenGLWidget>
namespace score::gfx
{
class ISFNode;
}
namespace Process
{
struct Preset;
}
namespace Gfx
{
class ShaderPreviewWidget : public QWidget
{
public:
  ShaderPreviewWidget(const QString& path, QWidget* parent = nullptr);
  ShaderPreviewWidget(const Process::Preset& path, QWidget* parent = nullptr);
  ~ShaderPreviewWidget();

private:
  void setup();
  void timerEvent(QTimerEvent* event) override;

  std::vector<score::gfx::Node*> m_previewInputs;
  std::unique_ptr<score::gfx::ISFNode> m_isf{};
  std::unique_ptr<score::gfx::ScreenNode> m_screen{};
  std::vector<std::unique_ptr<score::gfx::Node>> m_textures;
  score::gfx::Graph m_graph{};
  ProcessedProgram m_program;
};

}
