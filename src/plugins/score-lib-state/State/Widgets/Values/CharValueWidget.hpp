#pragma once
#include "ValueWidget.hpp"

#include <State/Value.hpp>

#include <ossia/network/domain/domain.hpp>

#include <QChar>
#include <QDialog>
class QLineEdit;
class QSpinBox;

namespace State
{
/**
 * @brief The CharValueWidget class A single char editor.
 *
 * It is a line edit limited to one character
 */
class SCORE_LIB_STATE_EXPORT CharValueWidget final : public ValueWidget
{
public:
  CharValueWidget(QChar value, QWidget* parent);

  ossia::value value() const override;
  void setValue(ossia::value v) const;

private:
  QLineEdit* m_value{};
};

/**
 * @brief The CharValueSetDialog class A dialog to allow to select multiple
 * chars
 */
class SCORE_LIB_STATE_EXPORT CharValueSetDialog final : public QDialog
{
public:
  using set_type = std::vector<char>;
  CharValueSetDialog(QWidget* parent);

  set_type values();

  void setValues(const set_type& t);

private:
  void addRow(char c);

  void removeRow(std::size_t i);

  QVBoxLayout* m_lay{};
  std::vector<QWidget*> m_rows;
  std::vector<CharValueWidget*> m_widgs;
};

/**
 * @brief The CharDomainWidget class Allows to select a domain for a Char
 */
class SCORE_LIB_STATE_EXPORT CharDomainWidget final : public QWidget
{
public:
  CharDomainWidget(QWidget* parent);

  ossia::domain_base<char> domain() const;
  void set_domain(ossia::domain);

private:
  QLineEdit* m_min{};
  QLineEdit* m_max{};

  std::vector<char> m_values;
};
}
