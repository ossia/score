#pragma once
#include <QWidget>

#include <score_lib_base_export.h>

class QFormLayout;

namespace score
{
class SCORE_LIB_BASE_EXPORT FormWidget : public QWidget
{
public:
  FormWidget(const QString& title, QWidget* parent = nullptr);
  ~FormWidget();

  QFormLayout* layout() const { return m_formLayout; }

private:
  QFormLayout* m_formLayout;
};
}
