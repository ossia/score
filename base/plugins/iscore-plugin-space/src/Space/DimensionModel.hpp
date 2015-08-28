#pragma once
#include <Space/bounded_symbol.hpp>
#include <QString>
#include <boost/optional.hpp>
#include <State/Value.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

// For now a simple min-max.
class DimensionModel : public IdentifiedObject<DimensionModel>
{
        Q_OBJECT
    public:
        DimensionModel(const QString& name, const Id<DimensionModel>& id, QObject* parent):
            IdentifiedObject{id, staticMetaObject.className(), parent},
            m_name{name},
            m_sym{GiNaC::symbol(name.toLatin1().constData()), spacelib::MinMaxDomain{}}
        {
        }

        const QString& name() const;
        const spacelib::minmax_symbol& sym() const;

        const auto& value() const { return m_val; }
        void setValue(const boost::optional<double>& val) { m_val = val; }

    private:
        QString m_name;
        spacelib::minmax_symbol m_sym;

        // The value is used if valid.
        boost::optional<double> m_val;
};
