#include "AreaParser.hpp"

// Maps to GiNaC::relational::operators
static const QStringList rels{"==", "!=", "<", "<=", ">", ">="};

GiNaC::relational::operators AreaParser::toOp(const QString& str)
{
    return static_cast<GiNaC::relational::operators>(rels.indexOf(str));
}

QStringList AreaParser::splitRelationship(const QString& eq)
{
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

    return res;
}

AreaParser::AreaParser(const QString& str)
{
    m_str = splitRelationship(str);
}

bool AreaParser::check() const
{
    return !m_str.empty();
}

std::unique_ptr<spacelib::area> AreaParser::result()
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
