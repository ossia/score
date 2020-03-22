#pragma once
#include <QTreeView>
#include <vector>
namespace Library
{
class LibraryInterface;
inline void setup_treeview(QTreeView& tv)
{
  tv.setHeaderHidden(true);
  tv.setDragEnabled(true);
  tv.setDropIndicatorShown(true);
  tv.setAlternatingRowColors(true);
  tv.setSelectionMode(QAbstractItemView::SingleSelection);
  for (int i = 1; i < tv.model()->columnCount(); ++i)
    tv.hideColumn(i);
}

std::vector<LibraryInterface*> libraryInterface(const QString& path);
}
