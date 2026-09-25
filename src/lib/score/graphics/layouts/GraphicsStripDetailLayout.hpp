#pragma once

#include <score/graphics/layouts/GraphicsBoxLayout.hpp>

#include <functional>

class QMimeData;
#include <vector>

namespace score
{
//! One cell of a strip: a small summary of a page, which selects it on click.
class SCORE_LIB_BASE_EXPORT GraphicsStripCell : public GraphicsVBoxLayout
{
public:
  explicit GraphicsStripCell(QGraphicsItem* parent);
  ~GraphicsStripCell();

  void setTitle(const QString& title);
  //! Shown instead of the title while not empty, e.g. the name of the sample
  //! of a drum. The cell keeps the size of its title: longer text is elided.
  void setShownTitle(const QString& title);
  void setSelected(bool selected);
  std::function<void()> onClicked;

  //! Lets files be dropped on the cell, e.g. a sample for the drum it
  //! stands for. accepts() says whether a drag's data would do.
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

//! Several pages of controls, only one shown at a time: a strip of summary
//! cells along the top selects the page below it, as the channel buttons of a
//! drum machine select the channel being edited. Every page keeps its place,
//! so the item does not change size when switching.
class SCORE_LIB_BASE_EXPORT GraphicsStripDetailLayout : public GraphicsLayout
{
public:
  explicit GraphicsStripDetailLayout(QGraphicsItem* parent);
  ~GraphicsStripDetailLayout();

  //! The layout the cells go in
  GraphicsLayout& strip() const noexcept { return *m_strip; }
  void addCell(GraphicsStripCell* cell);

  int currentIndex() const noexcept { return m_currentIndex; }
  void setCurrentIndex(int index);
  //! Called when a cell of the strip is clicked
  std::function<void(int)> onCurrentIndexChanged;
  //! Without its strip, the page shown is chosen from elsewhere (e.g. the
  //! rows of a table)
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
