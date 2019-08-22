#pragma once
#include <score_lib_base_export.h>
#include <QDoubleSpinBox>
namespace score
{
struct SCORE_LIB_BASE_EXPORT DoubleSpinboxWithEnter final
    : public QDoubleSpinBox
{
public:
  using QDoubleSpinBox::QDoubleSpinBox;

private:
  bool event(QEvent* event) override;
};

}
