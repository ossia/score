#pragma once
#include "ValueWidget.hpp"

#include <State/Value.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QDialog>

class QLineEdit;
class QWidget;

namespace State
{
class SCORE_LIB_STATE_EXPORT StringValueWidget final : public ValueWidget
{
public:
  StringValueWidget(const std::string& value, QWidget* parent = nullptr);

  ossia::value value() const override;

private:
  QLineEdit* m_value{};
};

class SCORE_LIB_STATE_EXPORT StringValueSetDialog final : public QDialog
{
public:
  using set_type = std::vector<std::string>;
  StringValueSetDialog(QWidget* parent);

  set_type values();

  void setValues(const set_type& t);

private:
  void addRow(const std::string& c);

  void removeRow(std::size_t i);

  QVBoxLayout* m_lay{};
  std::vector<QWidget*> m_rows;
  std::vector<StringValueWidget*> m_widgs;
};
}
