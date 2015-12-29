#include "AreaParser.hpp"

// Maps to GiNaC::relational::operators
static const QStringList operator_map_rels{"==", "!=", "<", "<=", ">", ">="}; // In the order of GiNaC::relational::operators
static const QStringList ordered_rels{"==", "!=", "<=", ">=", "<", ">"}; // Else parsing fails due to < matching before <=

static auto toOp(const QString& str)
{
    return static_cast<GiNaC::relational::operators>(operator_map_rels.indexOf(str));
}

std::pair<QStringList, GiNaC::relational::operators> splitRelationship(const QString& eq)
{
    QString found_rel;
    QStringList res;

    for(const QString& rel : ordered_rels)
    {
        if(eq.contains(rel))
        {
            res = eq.split(rel);
            found_rel = rel;
            break;
        }
    }

    if(res.size() == 2)
        return std::make_pair(res, toOp(found_rel));
    return {};
}

AreaParser::AreaParser(const QStringList& list)
{
    for(auto str : list)
        m_parsed.push_back(splitRelationship(str));
}

bool AreaParser::check() const
{
    return !m_parsed.empty() && !m_parsed.front().first.empty();
}

std::unique_ptr<spacelib::area> AreaParser::result()
{
    std::vector<GiNaC::relational> rels;
    std::vector<GiNaC::symbol> syms;
    GiNaC::parser parser;
    for(const auto& elt : m_parsed)
    {
        const auto& str = elt.first;
        const auto& op = elt.second;
        GiNaC::ex lhs, rhs;
        if(!str.empty())
        {
            lhs = parser(str[0].toStdString());
            rhs = parser(str[1].toStdString());
        }

        rels.emplace_back(lhs, rhs, op);
    }

    // Get all the variables.
    for(const auto& sym : parser.get_syms())
    { syms.push_back(GiNaC::ex_to<GiNaC::symbol>(sym.second)); }

    return std::make_unique<spacelib::area>(
                std::move(rels),
                syms);
}
