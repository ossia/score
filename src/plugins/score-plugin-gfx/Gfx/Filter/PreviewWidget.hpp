#pragma once
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/Window.hpp>
#include <Gfx/ShaderProgram.hpp>

#include <QWidget>
#include <QHBoxLayout>
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
class ShaderPreviewManager;
class ShaderPreviewWidget : public QWidget
{
public:
  ShaderPreviewWidget(const QString& path, QWidget* parent = nullptr);
  ShaderPreviewWidget(const Process::Preset& path, QWidget* parent = nullptr);
  ~ShaderPreviewWidget();

private:
  void setup();
  void timerEvent(QTimerEvent* event) override;

  std::shared_ptr<QWindow> m_window;
};

}
