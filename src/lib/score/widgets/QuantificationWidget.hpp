#pragma once
#include <ossia/editor/scenario/time_signature.hpp>

#include <QComboBox>
#include <QLineEdit>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QuantificationWidget : public QComboBox
{
  W_OBJECT(QuantificationWidget)
public:
  explicit QuantificationWidget(QWidget* parent = nullptr);

  double quantification() const noexcept;
  void setQuantification(double d);

  void quantificationChanged(double d)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, quantificationChanged, d)
};

class SCORE_LIB_BASE_EXPORT TimeSignatureWidget : public QLineEdit
{
public:
  explicit TimeSignatureWidget();

  void setSignature(std::optional<ossia::time_signature> t);

  std::optional<ossia::time_signature> signature() const;
};

}
