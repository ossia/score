#pragma once
#include <Gfx/Window/WindowSettings.hpp>

#include <QImage>
#include <QObject>
#include <QWidget>

#include <functional>
#include <vector>

namespace Gfx
{

enum class PreviewContent
{
  Black,
  PerOutputTestCard,
  GlobalTestCard,
  OutputIdentification
};

class PreviewWidget;
class OutputPreviewWindows final : public QObject
{
public:
  explicit OutputPreviewWindows(QObject* parent = nullptr);
  ~OutputPreviewWindows();

  void syncToMappings(const std::vector<OutputMapping>& mappings);
  void setPreviewContent(PreviewContent mode);
  void setInputResolution(QSize sz);
  void setSyncPositions(bool sync);

  // Called when a preview window's fullscreen state is toggled via double-click
  std::function<void(int, bool)> onFullscreenToggled;

private:
  void closeAll();
  void rebuildGlobalTestCard();

  std::vector<PreviewWidget*> m_windows;
  PreviewContent m_content{PreviewContent::Black};
  QSize m_inputResolution{1920, 1080};
  QImage m_globalTestCard;
  bool m_syncPositions{true};
};
class PreviewWidget final : public QWidget
{
public:
  explicit PreviewWidget(int index, PreviewContent content, QWidget* parent = nullptr);

  void setOutputIndex(int idx);
  void setPreviewContent(PreviewContent mode);
  void setOutputResolution(QSize sz);
  void setBlend(EdgeBlend left, EdgeBlend right, EdgeBlend top, EdgeBlend bottom);
  void setSourceRect(QRectF rect);
  void setCornerWarp(const CornerWarp& warp);
  void setTransform(int rotation, bool mirrorX, bool mirrorY);
  void setGlobalTestCard(const QImage& img);

  // Called when fullscreen is toggled via double-click (index, isFullscreen)
  std::function<void(int, bool)> onFullscreenToggled;

protected:
  void paintEvent(QPaintEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
  int m_index{};
  PreviewContent m_content{PreviewContent::Black};
  QSize m_resolution{1280, 720};
  QRectF m_sourceRect{0, 0, 1, 1};
  QImage m_globalTestCard;
  EdgeBlend m_blendLeft;
  EdgeBlend m_blendRight;
  EdgeBlend m_blendTop;
  EdgeBlend m_blendBottom;
  CornerWarp m_cornerWarp;
  int m_rotation{0};
  bool m_mirrorX{false};
  bool m_mirrorY{false};
};
}
