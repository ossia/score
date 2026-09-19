#include <Gfx/Widgets/CameraPreviewWidget.hpp>

#include <Gfx/Graph/BackgroundNode.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/VideoNode.hpp>
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Widgets/RhiPreviewWidget.hpp>

#include <Video/ExternalInput.hpp>

#include <QVBoxLayout>

namespace Gfx
{
namespace
{
constexpr int kProcessIntervalMs = 16;
}

CameraPreviewWidget::CameraPreviewWidget(QWidget* parent)
    : QWidget{parent}
{
  auto lay = new QVBoxLayout{this};
  lay->setContentsMargins(0, 0, 0, 0);
  m_graph = std::make_unique<score::gfx::Graph>();
  m_rhi = new RhiPreviewWidget{this};
  lay->addWidget(m_rhi);
}

CameraPreviewWidget::~CameraPreviewWidget()
{
  clear();
}

const Video::VideoMetadata* CameraPreviewWidget::metadata() const noexcept
{
  return m_input.get();
}

void CameraPreviewWidget::setInput(std::shared_ptr<Video::ExternalInput> input)
{
  clear();
  if(!input)
    return;

  m_input = std::move(input);
  if(!m_input->start())
  {
    m_input.reset();
    return;
  }

  m_node = std::make_unique<score::gfx::CameraNode>(m_input);
  m_node->setScaleMode(score::gfx::ScaleMode::BlackBars);
  m_graph->addNode(m_node.get());
  attach();

  if(m_timerId == 0)
    m_timerId = startTimer(kProcessIntervalMs);
}

void CameraPreviewWidget::clear()
{
  if(m_timerId)
  {
    killTimer(m_timerId);
    m_timerId = 0;
  }

  // Before the node goes, since the preview's render list holds it -- and
  // before the graph goes, since ~QWidget deletes this child only after our
  // members are destroyed. detach() drops its graph pointer, so that later
  // destruction finds nothing to do.
  m_rhi->detach();

  if(m_node)
  {
    m_graph->removeNode(m_node.get());
    m_node.reset();
  }

  if(m_input)
  {
    m_input->stop();
    m_input.reset();
  }
}

void CameraPreviewWidget::attach()
{
  m_rhi->useGraph(
      m_graph.get(),
      [this](score::gfx::BackgroundNode& n) {
        if(!m_node)
          return;
        m_graph->addEdge(
            m_node->output[0], n.input[0], Process::CableType::ImmediateGlutton);
        m_graph->createAllRenderLists(Gfx::Settings::graphicsApiForCurrentApplication());
      },
      [this](score::gfx::BackgroundNode& n) {
        if(m_node)
          m_graph->removeEdge(m_node->output[0], n.input[0]);
      });
}

void CameraPreviewWidget::timerEvent(QTimerEvent* ev)
{
  if(ev->timerId() != m_timerId)
  {
    QWidget::timerEvent(ev);
    return;
  }

  if(m_node)
    m_node->process(score::gfx::Message{});
}
}
