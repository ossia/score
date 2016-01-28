#include "Value.hpp"

class QDebug;

bool State::Value::operator==(const State::Value& m) const
{
    return val == m.val;
}

bool State::Value::operator!=(const State::Value& m) const
{
    return val != m.val;
}

bool State::Value::operator<(const State::Value& m) const
{
    return false;
}

State::ValueImpl::ValueImpl(State::no_value_t v): m_variant{v} { }
State::ValueImpl::ValueImpl(State::impulse_t v): m_variant{v} { }
State::ValueImpl::ValueImpl(int v): m_variant{v} { }
State::ValueImpl::ValueImpl(float v): m_variant{v} { }
State::ValueImpl::ValueImpl(double v): m_variant{(float)v} { }
State::ValueImpl::ValueImpl(bool v): m_variant{v} { }
State::ValueImpl::ValueImpl(const QString& v): m_variant{v} { }
State::ValueImpl::ValueImpl(QChar v): m_variant{v} { }
State::ValueImpl::ValueImpl(const State::tuple_t& v): m_variant{v} { }


State::ValueImpl::ValueImpl():
    m_variant{no_value_t{}}
{

}

State::ValueImpl& State::ValueImpl::operator=(State::no_value_t v) { m_variant = v; return *this; }
State::ValueImpl& State::ValueImpl::operator=(State::impulse_t v) { m_variant = v; return *this; }
State::ValueImpl& State::ValueImpl::operator=(int v) { m_variant = v; return *this;  }
State::ValueImpl& State::ValueImpl::operator=(float v) { m_variant = v; return *this;  }
State::ValueImpl& State::ValueImpl::operator=(double v) { m_variant = (float)v; return *this;  }
State::ValueImpl& State::ValueImpl::operator=(bool v) { m_variant = v; return *this;  }
State::ValueImpl& State::ValueImpl::operator=(const QString& v) { m_variant = v; return *this;  }
State::ValueImpl& State::ValueImpl::operator=(QChar v) { m_variant = v; return *this;  }
State::ValueImpl& State::ValueImpl::operator=(const State::tuple_t& v) { m_variant = v; return *this;  }


bool State::ValueImpl::operator ==(const State::ValueImpl& other) const
{
    return m_variant == other.m_variant;
}

bool State::ValueImpl::operator !=(const State::ValueImpl& other) const
{
    return m_variant != other.m_variant;
}

bool State::ValueImpl::isNumeric() const
{
    auto t = m_variant.which();
    return t == 1 || t == 2;
}

bool State::ValueImpl::isValid() const
{
    return m_variant.which() != m_variant.npos;
}


namespace State {

ISCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const State::ValueImpl& m)
{
    return s;
}
}
