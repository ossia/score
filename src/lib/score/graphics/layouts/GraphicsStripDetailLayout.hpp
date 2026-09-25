#pragma once

#include <score/graphics/layouts/GraphicsBoxLayout.hpp>

#include <functional>

class QMimeData;
#include <vector>

namespace score
{
//! Summary of a page, selects it on click
class SCORE_LIB_BASE_EXPORT GraphicsStripCell : public GraphicsVBoxLayout
{
public:
  explicit GraphicsStripCell(QGraphicsItem* parent);
  ~GraphicsStripCell();

  void setTitle(const QString& title);
  //! Replaces the title while not empty. Elided, does not resize the cell.
  void setShownTitle(const QString& title);
  void setSelected(bool selected);
  std::function<void()> onClicked;

  void setDropHandler(
      std::function<bool(const QMimeData&)> accepts,
      std::function<void(const QMimeData&)> drop);
  bool acceptsDrop(const QMimeData& data) const;
  void drop(const QMimeData& data);
  void setDropHighlight(bool on);

  void layout() override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

private:
  qreal titleHeight() const;

  QString m_title;
  QString m_shownTitle;
  QGraphicsItem* m_hitArea{};
  std::function<bool(const QMimeData&)> m_acceptsDrop;
  std::function<void(const QMimeData&)> m_drop;
  bool m_selected{};
  bool m_dropHighlight{};
};

//! Pages shown one at a time, selected from a strip of summary cells.
//! Sized for the largest page.
class SCORE_LIB_BASE_EXPORT GraphicsStripDetailLayout : public GraphicsLayout
{
public:
  explicit GraphicsStripDetailLayout(QGraphicsItem* parent);
  ~GraphicsStripDetailLayout();

  GraphicsLayout& strip() const noexcept { return *m_strip; }
  void addCell(GraphicsStripCell* cell);

  int currentIndex() const noexcept { return m_currentIndex; }
  void setCurrentIndex(int index);
  //! On cell click
  std::function<void(int)> onCurrentIndexChanged;
  //! Hide when the page is selected elsewhere, e.g. by table rows
  void setStripVisible(bool visible);

  void layout() override;

private:
  std::vector<QGraphicsItem*> pages() const;

  GraphicsHBoxLayout* m_strip{};
  std::vector<GraphicsStripCell*> m_cells;
  int m_currentIndex{};
  bool m_stripVisible{true};
};
}
