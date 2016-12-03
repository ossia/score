#pragma once
#include <QChar>
#include <QDialog>

#include "ValueWidget.hpp"
#include <ossia/network/domain/domain.hpp>
#include <State/Value.hpp>
class QLineEdit;
class QSpinBox;

namespace State
{
/**
 * @brief The CharValueWidget class A single char editor.
 *
 * It is a line edit limited to one character
 */
class ISCORE_LIB_STATE_EXPORT CharValueWidget final : public State::ValueWidget
{
public:
  CharValueWidget(QChar value, QWidget* parent);

  State::Value value() const override;
  void setValue(State::Value v) const;

private:
  QLineEdit* m_value{};
};

/**
 * @brief The CharValueSetDialog class A dialog to allow to select multiple
 * chars
 */
class ISCORE_LIB_STATE_EXPORT CharValueSetDialog final : public QDialog
{
public:
  using set_type = boost::container::flat_set<char>;
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
class ISCORE_LIB_STATE_EXPORT CharDomainWidget final : public QWidget
{
public:
  CharDomainWidget(QWidget* parent);

  ossia::net::domain_base<char> domain() const;
  void setDomain(ossia::net::domain);

private:
  QLineEdit* m_min{};
  QLineEdit* m_max{};

  boost::container::flat_set<char> m_values;
};
}
