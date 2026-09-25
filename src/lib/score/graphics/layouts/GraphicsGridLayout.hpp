#pragma once

#include <score/graphics/GraphicsLayout.hpp>

#include <QStringList>

#include <vector>

namespace score
{

class SCORE_LIB_BASE_EXPORT GraphicsGridColumnsLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsGridColumnsLayout();

  void setColumns(int columns);

  void layout() override;

private:
  int m_columns{5};
};

class SCORE_LIB_BASE_EXPORT GraphicsGridRowsLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsGridRowsLayout();

  void setRows(int rows);

  void layout() override;

private:
  int m_rows{5};
};

//! Rows (row layouts of cells) under shared column titles.
//! With row titles, column titles start at the second column.
class SCORE_LIB_BASE_EXPORT GraphicsTableLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsTableLayout();

  void setColumnTitles(QStringList titles);
  void setRowTitles(bool rowTitles);
  void setTitle(QString title);
  //! Rows must be GraphicsSelectableRow
  void setRowsSelectable(bool selectable);

  void layout() override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;

protected:
  bool sceneEventFilter(QGraphicsItem* watched, QEvent* event) override;

private:
  static QFont titleFont();
  QStringList m_titles;
  QString m_title;
  std::vector<std::pair<double, double>> m_columns; // x, width
  bool m_rowTitles{};
  bool m_rowsSelectable{};
};

class SCORE_LIB_BASE_EXPORT GraphicsDefaultLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsDefaultLayout();

  void layout() override;
};

class SCORE_LIB_BASE_EXPORT GraphicsDefaultInletLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsDefaultInletLayout();

  void layout() override;
};

class SCORE_LIB_BASE_EXPORT GraphicsDefaultOutletLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsDefaultOutletLayout();

  //! Outlets belong against the right-hand edge of the node. The column lays
  //! itself out against this width when the node is wider than the column,
  //! which is what GraphicsIORootLayout does for a node that also has inlets.
  void setMinimumWidth(double w);
  void layout() override;

private:
  double m_minimumWidth{};
};

class SCORE_LIB_BASE_EXPORT GraphicsIORootLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsIORootLayout();

  void setMinimumWidth(double w);
  void layout() override;

private:
  double m_minimumWidth{};
};
}
