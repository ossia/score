#pragma once
#include <QToolButton>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT ArrowButton : public QToolButton
{
  W_OBJECT(ArrowButton)
public:
  ArrowButton(Qt::ArrowType arrowType, QWidget* parent = 0);

  void setArrowType(Qt::ArrowType type);

  Qt::ArrowType arrowType () const {return m_arrowType;}

private:
  Qt::ArrowType m_arrowType;
};
}
