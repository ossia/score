#pragma once
#include <Gfx/Window/WindowSettings.hpp>

#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace Gfx
{

class OutputMappingCanvas;

// Graphics item for draggable/resizable output mapping quads
class OutputMappingItem final : public QGraphicsRectItem
{
public:
  explicit OutputMappingItem(
      int index, const QRectF& rect, OutputMappingCanvas* canvas,
      QGraphicsItem* parent = nullptr);

  int outputIndex() const noexcept { return m_index; }
  void setOutputIndex(int idx);

  // Per-output window properties stored on the item
  int screenIndex{-1};
  QPoint windowPosition{0, 0};
  QSize windowSize{1280, 720};
  bool fullscreen{false};

  // Soft-edge blending
  EdgeBlend blendLeft;
  EdgeBlend blendRight;
  EdgeBlend blendTop;
  EdgeBlend blendBottom;

  // 4-corner perspective warp
  CornerWarp cornerWarp;

  OutputLockMode lockMode{OutputLockMode::Free};

  int rotation{0};
  bool mirrorX{false};
  bool mirrorY{false};

  void applyLockedState();

  // Called when item is moved or resized in the canvas
  std::function<void()> onChanged;

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;

private:
  enum ResizeEdge
  {
    None = 0,
    Left = 1,
    Right = 2,
    Top = 4,
    Bottom = 8
  };
  int hitTestEdges(const QPointF& pos) const;

  enum BlendHandle
  {
    BlendNone = 0,
    BlendLeft,
    BlendRight,
    BlendTop,
    BlendBottom
  };
  BlendHandle hitTestBlendHandles(const QPointF& pos) const;

  int m_index{};
  int m_resizeEdges{None};
  BlendHandle m_blendHandle{BlendNone};
  QPointF m_dragStart{};
  QRectF m_rectStart{};
  QPointF m_moveAnchorScene{}; // scene pos at press for precision move
  QPointF m_posAtPress{};      // item pos() at press
  OutputMappingCanvas* m_canvas{};
};

class OutputMappingCanvas final : public QGraphicsView
{
public:
  explicit OutputMappingCanvas(QWidget* parent = nullptr);
  ~OutputMappingCanvas();

  void setMappings(const std::vector<OutputMapping>& mappings);
  std::vector<OutputMapping> getMappings() const;

  void addOutput();
  void removeSelectedOutput();

  void updateAspectRatio(int inputWidth, int inputHeight);

  double canvasWidth() const noexcept { return m_canvasWidth; }
  double canvasHeight() const noexcept { return m_canvasHeight; }

  bool snapEnabled() const noexcept { return m_snapEnabled; }
  void setSnapEnabled(bool enabled);
  QPointF snapPosition(const OutputMappingItem* item, QPointF proposedPos) const;

  // Warp mode: double-click an item to enter/exit
  void enterWarpMode(int outputIndex);
  void exitWarpMode();
  bool inWarpMode() const noexcept { return m_warpItemIndex >= 0; }
  void resetWarp();

  std::function<void(int)> onSelectionChanged;
  std::function<void(int)> onItemGeometryChanged;
  std::function<void()> onWarpChanged;

protected:
  void resizeEvent(QResizeEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;

private:
  void setupItemCallbacks(OutputMappingItem* item);
  void updateWarpVisuals();
  OutputMappingItem* findItemByIndex(int index) const;

  QGraphicsScene m_scene;
  QGraphicsRectItem* m_border{};
  double m_canvasWidth{400.0};
  double m_canvasHeight{300.0};
  bool m_snapEnabled{true};

  // Warp mode state
  int m_warpItemIndex{-1};
  QGraphicsEllipseItem* m_warpHandles[4]{};
  QGraphicsPolygonItem* m_warpQuad{};
  std::vector<QGraphicsLineItem*> m_warpGrid;
  bool m_warpDragging{false};
  int m_warpDragHandle{-1};
  QPointF m_warpHandleAnchor{};
  QPointF m_warpMouseAnchor{};
};
}
