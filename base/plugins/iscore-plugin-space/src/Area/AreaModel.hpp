#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <Space/area.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>
class SpaceModel;

// in the end, isn't an area the same thing as a domain???
class AreaParser
{
        QStringList m_str;
        GiNaC::ex m_lhs, m_rhs;
        GiNaC::relational::operators m_op;

    private:

        static GiNaC::relational::operators toOp(const QString& str)
        {
            static const QStringList rels{"==", "!=", "<", "<=", ">", ">="};

            return static_cast<GiNaC::relational::operators>(rels.indexOf(str));
        }

        QStringList splitRelationship(const QString& eq)
        {
            static const QStringList rels{"==", "!=", "<=", ">=", "<", ">"};
            QString found_rel;
            QStringList res;

            for(const QString& rel : rels)
            {
                if(eq.contains(rel))
                {
                    res = eq.split(rel);
                    found_rel = rel;
                    break;
                }
            }

            if(res.size() != 2)
                return {};


            m_op = toOp(found_rel);

            qDebug() <<"split:" << res;
            return res;
        }

    public:
        AreaParser(const QString& str)
        {
            m_str = splitRelationship(str);
        }

        bool check() const
        { return !m_str.empty(); }

        std::unique_ptr<spacelib::area> result()
        {
            GiNaC::parser lhs_p, rhs_p;
            if(!m_str.empty())
            {
                m_lhs = lhs_p(m_str[0].toStdString());
                m_rhs = rhs_p(m_str[1].toStdString());
            }

            // Get all the variables.
            std::vector<GiNaC::symbol> syms;
            for(const auto& sym : lhs_p.get_syms())
            { syms.push_back(GiNaC::ex_to<GiNaC::symbol>(sym.second)); }
            for(const auto& sym : rhs_p.get_syms())
            { syms.push_back(GiNaC::ex_to<GiNaC::symbol>(sym.second)); }

            return std::make_unique<spacelib::area>(
                        GiNaC::relational(m_lhs, m_rhs, m_op),
                        syms);
        }
};

// Maps addresses / values to the parameter of an area
class AreaModel : public IdentifiedObject<AreaModel>
{
        Q_OBJECT
    public:
        AreaModel(
                std::unique_ptr<spacelib::area>&& area,
                const SpaceModel& space,
                const id_type<AreaModel>&,
                QObject* parent);

        void setArea(std::unique_ptr<spacelib::area> &&ar);
        void setSpaceMapping(const GiNaC::exmap& mapping);

        const spacelib::area& area() const
        { return *m_area; }
        spacelib::projected_area projectedArea() const;
        spacelib::valued_area valuedArea() const;

        const SpaceModel& space() const
        { return m_space; }
        const auto& spaceMapping() const
        { return m_spaceMapping; }

        void mapAddressToParameter(const QString& str,
                                   const iscore::FullAddressSettings& addr);

        void mapValueToParameter(const QString& str,
                                 const iscore::Value&& val);

    signals:
        void areaChanged();

    private:
        const SpaceModel& m_space;
        std::unique_ptr<spacelib::area> m_area;

        GiNaC::exmap m_spaceMapping;

        // bool is true if we use only the value, false if we
        // use the whole address
        QMap<QString,
             QPair<GiNaC::symbol, QPair<bool, iscore::FullAddressSettings>>
        > m_addressMap;
};
