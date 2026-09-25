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

//! Rows of controls under shared column titles. Each child is a row layout
//! whose children are its cells; every column is as wide as its widest cell,
//! every row as tall as its tallest. When rows start with a title cell, the
//! column titles start at the second column.
class SCORE_LIB_BASE_EXPORT GraphicsTableLayout : public GraphicsLayout
{
public:
  using GraphicsLayout::GraphicsLayout;
  ~GraphicsTableLayout();

  void setColumnTitles(QStringList titles);
  void setRowTitles(bool rowTitles);
  //! Drawn above the row titles, naming the whole table.
  void setTitle(QString title);
  //! Rows are GraphicsSelectableRow: a press on their background selects
  //! them (presses on their controls are caught by the rows themselves).
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
