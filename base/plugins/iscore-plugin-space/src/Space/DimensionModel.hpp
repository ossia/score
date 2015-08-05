#pragma once
#include <Space/bounded_symbol.hpp>
#include <QString>
#include <boost/optional.hpp>
#include <State/Value.hpp>

// For now a simple min-max.
class DimensionModel
{
    public:
        DimensionModel(const QString& name):
            m_name{name},
            m_sym{GiNaC::symbol(name.toLatin1().constData()), spacelib::MinMaxDomain{}}
        {
        }

        const QString& name() const;
        spacelib::minmax_symbol& sym();
        const spacelib::minmax_symbol& sym() const;

        const auto& value() const { return m_val; }
        void setValue(const boost::optional<double>& val) { m_val = val; }
    private:
        QString m_name;
        spacelib::minmax_symbol m_sym;

        // The value is used if valid.
        boost::optional<double> m_val;
};
