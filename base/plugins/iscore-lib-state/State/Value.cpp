#include "Value.hpp"
#include <State/ValueConversion.hpp>

namespace State {
bool Value::operator==(const Value& m) const
{
    return val == m.val;
}

bool Value::operator!=(const Value& m) const
{
    return val != m.val;
}

ValueImpl::ValueImpl(): m_variant{no_value_t{}} { }
ValueImpl::ValueImpl(no_value_t v): m_variant{v} { }
ValueImpl::ValueImpl(impulse_t v): m_variant{v} { }
ValueImpl::ValueImpl(int v): m_variant{v} { }
ValueImpl::ValueImpl(float v): m_variant{v} { }
ValueImpl::ValueImpl(double v): m_variant{(float)v} { }
ValueImpl::ValueImpl(bool v): m_variant{v} { }
ValueImpl::ValueImpl(QString v): m_variant{std::move(v)} { }
ValueImpl::ValueImpl(QChar v): m_variant{v} { }
ValueImpl::ValueImpl(vec2f v): m_variant{v} { }
ValueImpl::ValueImpl(vec3f v): m_variant{v} { }
ValueImpl::ValueImpl(vec4f v): m_variant{v} { }
ValueImpl::ValueImpl(tuple_t v): m_variant{std::move(v)} { }

ValueImpl& ValueImpl::operator=(no_value_t v) { m_variant = v; return *this; }
ValueImpl& ValueImpl::operator=(impulse_t v) { m_variant = v; return *this; }
ValueImpl& ValueImpl::operator=(int v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(float v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(double v) { m_variant = (float)v; return *this;  }
ValueImpl& ValueImpl::operator=(bool v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(const QString& v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(QString&& v) { m_variant = std::move(v); return *this;  }
ValueImpl& ValueImpl::operator=(QChar v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(vec2f v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(vec3f v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(vec4f v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(const tuple_t& v) { m_variant = v; return *this;  }
ValueImpl& ValueImpl::operator=(tuple_t&& v) { m_variant = std::move(v); return *this;  }


bool ValueImpl::operator ==(const ValueImpl& other) const
{
    return m_variant == other.m_variant;
}

bool ValueImpl::operator !=(const ValueImpl& other) const
{
    return m_variant != other.m_variant;
}

bool ValueImpl::isNumeric() const
{
    auto t = m_variant.which();
    return t == 1 || t == 2;
}

bool ValueImpl::isValid() const
{
    return m_variant.which() != m_variant.npos;
}

bool ValueImpl::isArray() const
{
    return is<tuple_t>()
            || is<vec2f>()
            || is<vec3f>()
            || is<vec4f>();
}

ISCORE_LIB_STATE_EXPORT QDebug& operator<<(QDebug& s, const Value& m)
{
    s << convert::textualType(m) << convert::toPrettyString(m);
    return s;
}

}
