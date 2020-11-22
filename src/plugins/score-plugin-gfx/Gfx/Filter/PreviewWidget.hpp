#pragma once
#include <Gfx/Graph/graph.hpp>
#include <Gfx/Graph/isfnode.hpp>
#include <Gfx/Graph/node.hpp>
#include <Gfx/Graph/nodes.hpp>
#include <Gfx/Graph/shadercache.hpp>
#include <Gfx/Graph/window.hpp>
#include <Gfx/Graph/imagenode.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <QHBoxLayout>
#include <QOpenGLWidget>

struct ISFNode;
namespace Gfx
{

class ShaderPreviewWidget: public QWidget
{
public:
  ShaderPreviewWidget(const QString& path, QWidget* parent = nullptr);

private:
  void setup();
  void timerEvent(QTimerEvent* event) override;

  Graph m_graph{};
  std::vector<score::gfx::Node*> m_previewInputs;
  ISFNode* m_isf{};
  ProcessedProgram m_program;
};
}
