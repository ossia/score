#pragma once

#include <State/Expression.hpp>

#include <QValidator>

// TODO move in state plugin
template <typename T>
class ExpressionValidator final : public QValidator
{
public:
  QValidator::State validate(QString& str, int&) const override
  {
    if (str.isEmpty())
    {
      // Remove the condition
      m_currentExp = T{};
      return QValidator::State::Acceptable;
    }
    else
    {
      m_currentExp = ::State::parseExpression(str);
      return m_currentExp ? QValidator::State::Acceptable : QValidator::State::Intermediate;
    }
  }

  std::optional<::State::Expression> get() const { return m_currentExp; }

private:
  mutable std::optional<::State::Expression> m_currentExp;
};
