#pragma once

#include <QValidator>
#include <State/Expression.hpp>

template<typename T>
class ExpressionValidator final : public QValidator
{
    public:
    State validate(QString& str, int&) const override
    {
        if(str.isEmpty())
        {
            // Remove the condition
            m_currentExp = T{};
            return State::Acceptable;
        }
        else
        {
            m_currentExp = iscore::parseExpression(str);
            return m_currentExp ? State::Acceptable : State::Intermediate;
        }
    }

    boost::optional<iscore::Expression> get() const
    {
        return m_currentExp;
    }

    private:
    mutable boost::optional<iscore::Expression> m_currentExp;
};
