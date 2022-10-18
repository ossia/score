#pragma once
#include <QDoubleSpinBox>
#include <QSpinBox>

#include <score_lib_base_export.h>
namespace score
{
struct SCORE_LIB_BASE_EXPORT SpinboxWithEnter final : public QSpinBox
{
public:
  using QSpinBox::QSpinBox;

private:
  bool event(QEvent* event) override;
};
struct SCORE_LIB_BASE_EXPORT DoubleSpinboxWithEnter final : public QDoubleSpinBox
{
public:
  using QDoubleSpinBox::QDoubleSpinBox;

private:
  bool event(QEvent* event) override;
};

}
