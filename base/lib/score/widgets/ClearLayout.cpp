// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClearLayout.hpp"

#include <QApplication>
#include <QLayout>
#include <QLayoutItem>
#include <QWidget>

void score::clearLayout(QLayout* layout)
{
  if (!layout)
    return;
  if (layout->count() == 0)
    return;

  QLayoutItem* child{};
  while ((child = layout->takeAt(0)) != nullptr)
  {
    if (child->layout() != nullptr)
      clearLayout(child->layout());
    else if (child->widget() != nullptr)
      delete child->widget();

    delete child;
  }
}

void score::setCursor(Qt::CursorShape c)
{
  if (qApp)
  {
    if (auto w = qApp->activeWindow())
    {
      w->setCursor(c);
    }
  }
}
