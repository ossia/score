#pragma once
#include <QAction>
#include <QWidget>

namespace score
{
inline void setHelp(QWidget* widg, const QString& shorttxt, const QString& txt)
{
  widg->setToolTip(shorttxt);
  widg->setStatusTip(txt);
  widg->setWhatsThis(txt);
}
inline void setHelp(QAction* widg, const QString& shorttxt, const QString& txt)
{
  widg->setToolTip(shorttxt);
  widg->setStatusTip(txt);
  widg->setWhatsThis(txt);
}
inline void setHelp(QWidget* widg, const QString& txt)
{
  widg->setToolTip(txt);
  widg->setStatusTip(txt);
  widg->setWhatsThis(txt);
}
inline void setHelp(QAction* widg, const QString& txt)
{
  widg->setToolTip(txt);
  widg->setStatusTip(txt);
  widg->setWhatsThis(txt);
}
}
