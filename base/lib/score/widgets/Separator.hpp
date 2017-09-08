#pragma once
#include <QWidget>
#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT HSeparator : public QWidget
{
public:
  explicit HSeparator(QWidget* parent);
  ~HSeparator();
};

class SCORE_LIB_BASE_EXPORT VSeparator : public QWidget
{
public:
  explicit VSeparator(QWidget* parent);
  ~VSeparator();
};
}
