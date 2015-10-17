#include "Value.hpp"

namespace iscore {
QDataStream& operator<<(QDataStream& s, const iscore::Value& m)
{
    s << m.val;
    return s;
}

QDataStream& operator>>(QDataStream& s, iscore::Value& m)
{
    s >> m.val;
    return s;
}
}

bool iscore::Value::operator==(const iscore::Value& m) const
{
    return val == m.val;
}

bool iscore::Value::operator!=(const iscore::Value& m) const
{
    return val != m.val;
}

bool iscore::Value::operator<(const iscore::Value& m) const
{
    return false;
}

iscore::ValueImpl::ValueImpl(iscore::no_value_t v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(iscore::impulse_t v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(int v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(float v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(double v): m_variant{(float)v} { }
iscore::ValueImpl::ValueImpl(bool v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(const QString& v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(QChar v): m_variant{v} { }
iscore::ValueImpl::ValueImpl(const iscore::tuple_t& v): m_variant{v} { }


iscore::ValueImpl::ValueImpl():
    m_variant{no_value_t{}}
{

}

iscore::ValueImpl& iscore::ValueImpl::operator=(iscore::no_value_t v) { m_variant = v; return *this; }
iscore::ValueImpl& iscore::ValueImpl::operator=(iscore::impulse_t v) { m_variant = v; return *this; }
iscore::ValueImpl& iscore::ValueImpl::operator=(int v) { m_variant = v; return *this;  }
iscore::ValueImpl& iscore::ValueImpl::operator=(float v) { m_variant = v; return *this;  }
iscore::ValueImpl& iscore::ValueImpl::operator=(double v) { m_variant = (float)v; return *this;  }
iscore::ValueImpl& iscore::ValueImpl::operator=(bool v) { m_variant = v; return *this;  }
iscore::ValueImpl& iscore::ValueImpl::operator=(const QString& v) { m_variant = v; return *this;  }
iscore::ValueImpl& iscore::ValueImpl::operator=(QChar v) { m_variant = v; return *this;  }
iscore::ValueImpl& iscore::ValueImpl::operator=(const iscore::tuple_t& v) { m_variant = v; return *this;  }


bool iscore::ValueImpl::operator ==(const iscore::ValueImpl& other) const
{
    return m_variant == other.m_variant;
}

bool iscore::ValueImpl::operator !=(const iscore::ValueImpl& other) const
{
    return m_variant != other.m_variant;
}

bool iscore::ValueImpl::isNumeric() const
{
    auto t = m_variant.which();
    return t == 1 || t == 2;
}

bool iscore::ValueImpl::isValid() const
{
    return m_variant.which() != m_variant.npos;
}


namespace iscore {
QDebug& operator<<(QDebug& s, const iscore::ValueImpl& m)
{
    return s;
}


QDataStream& operator<<(QDataStream& s, const iscore::ValueImpl& m)
{
    return s;
}


QDataStream& operator>>(QDataStream& s, iscore::ValueImpl& m)
{
    return s;
}
}
