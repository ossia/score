#pragma once
#include <Space/area.hpp>
#include <QStringList>
class AreaParser
{
    public:
        AreaParser(const QString& str);

        bool check() const;
        std::unique_ptr<spacelib::area> result();


    private:
        static GiNaC::relational::operators toOp(const QString& str);
        QStringList splitRelationship(const QString& eq);

        QStringList m_str;
        GiNaC::ex m_lhs, m_rhs;
        GiNaC::relational::operators m_op;
};

// TODO AreaValidator for QLineEdit..
