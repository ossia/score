#include "Expression.hpp"
#include <algorithm>
/*
bool checkLeaves(const iscore::Expression* e)
{
    auto c = e->children(); // TODO see why this isn't a const ref return.
    if(c.isEmpty())
    {
        return e->is<iscore::Relation>();
    }
    else
    {
        return std::all_of(
                    c.cbegin(), c.cend(),
                    [] (auto e) {
            return checkLeaves(e);
        });
    }
}

bool validate(const iscore::Expression& expr)
{
    // Check that all the leaves are relations.
    return checkLeaves(&expr);
}
*/
/*
void test_parse_addr()
{
    using namespace qi;


    const QStringList rels{"==", "!=", ">", "<", ">=", "<="};



    std::string str("minuit:/device/lol");

    typedef std::string::const_iterator iterator_type;
    using qi::parse;
    using ascii::space;

    address_parser<iterator_type> parser;
    auto first = str.cbegin(), last = str.cend();
    iscore::Address addr;
    bool r = parse(first, last, parser, addr);

    qDebug() << "parsed?" << r;
    qDebug() << addr.device;
    for(auto& elt : addr.path)
        qDebug() << elt;
    //if (first!=last) std::cerr << "unparsed: '" << std::string(first, last) << "'\n";
}

void test_parse_value()
{
    std::vector<std::string> str_list{
        "[1,2,3]",
        "[1]",
        "[ 1 ]",
        "[ 1, 2, 3 ]",
        "[ 1, 2.3, 3, 'c' ]",
        "1",
        "1.23",
        "'c'",
        "\"lala\"",
        "\"lala lala\""
    };

    for(const auto& str : str_list)
    {

        typedef std::string::const_iterator iterator_type;
        using qi::parse;

        value_parser<iterator_type> parser;
        auto first = str.cbegin(), last = str.cend();
        iscore::Value val;
        bool r = parse(first, last, parser, val);

        qDebug() << str.c_str() << r << val.val << "                    ";
    }
}


void test_parse_rel_member()
{
    std::vector<std::string> str_list{
        "minuit:/device"
        "1234"
    };

    for(const auto& str : str_list)
    {
        typedef std::string::const_iterator iterator_type;
        using qi::parse;

        RelationMember_parser<iterator_type> parser;
        auto first = str.cbegin(), last = str.cend();
        iscore::RelationMember val;
        bool r = parse(first, last, parser, val);

        qDebug() << str.c_str() << r << val.which();
    }
}

void test_parse_rel()
{
    std::vector<std::string> str_list{
        "minuit:/device<=1234",
        "minuit:/device <= 1234"
    };

    for(const auto& str : str_list)
    {
        typedef std::string::const_iterator iterator_type;
        using qi::parse;

        Relation_parser<iterator_type> parser;
        auto first = str.cbegin(), last = str.cend();
        iscore::Relation val;
        bool r = parse(first, last, parser, val);

        qDebug() << str.c_str() << r << val.lhs.target<iscore::Address>()->path << val.rhs.target<iscore::Value>()->val << "                    ";
    }
}


void test_parse_expr()
{
    for (auto& input : std::list<std::string> {
         "(dev:/minuit <= 5) and (a:/b == 1.34);",
         "(dev:/minuit != [1, 2, 3.12, 'c']) and not (a:/b >= c:/d/e/f);"
   })
    {
        auto f(std::begin(input)), l(std::end(input));
        parser<decltype(f)> p;

        try
        {
            expr result;
            bool ok = qi::phrase_parse(f,l,p > ';',qi::space,result);

            if (!ok)
                qDebug() << "invalid input\n";
            //else
            //    qDebug() << "result: " << result << "\n";

        } catch (const qi::expectation_failure<decltype(f)>& e)
        {
            qDebug() << "expectation_failure at '" << QString::fromStdString(std::string(e.first, e.last)) << "'\n";
        }

        if (f!=l) qDebug() << "unparsed: '" << QString::fromStdString(std::string(f,l)) << "'\n";
    }

    exit(0);
}
void expr_parse_test()
{
    test_parse_addr();
    test_parse_value();

    test_parse_rel_member();
    test_parse_rel();
    test_parse_expr();
}

QDebug operator<<(QDebug dbg, const iscore::Address& a)
{
    dbg << a.toString()
    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::Value& v)
{
    dbg << v.val;
    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::RelationMember& v)
{
    using namespace eggs::variants;
    switch(v.which())
    {
        case 0:
            dbg << get<iscore::Address>(v);
            break;
        case 1:
            dbg << get<iscore::Value>(v);
            break;
    }

    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::Relation::Operator& v)
{
    switch(v)
    {
        case iscore::Relation::Operator::Different:
            dbg << "!="; break;
        case iscore::Relation::Operator::Equal:
            dbg << "=="; break;
        case iscore::Relation::Operator::Greater:
            dbg << ">"; break;
        case iscore::Relation::Operator::GreaterEqual:
            dbg << ">="; break;
        case iscore::Relation::Operator::Lower:
            dbg << "<"; break;
        case iscore::Relation::Operator::LowerEqual:
            dbg << "<="; break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::Relation& v)
{
    dbg << v.lhs << v.op << v.rhs;
    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::BinaryOperator& v)
{
    switch(v)
    {
        case iscore::BinaryOperator::And:
            dbg << "and"; break;
        case iscore::BinaryOperator::Or:
            dbg << "or"; break;
        case iscore::BinaryOperator::Xor:
            dbg << "xor"; break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::UnaryOperator& v)
{
    switch(v)
    {
        case iscore::UnaryOperator::Not:
            dbg << "not"; break;
    }
    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::ExprData& v)
{
    if(v.is<iscore::Relation>())
        dbg << v.get<iscore::Relation>();
    else if(v.is<iscore::BinaryOperator>())
        dbg << v.get<iscore::BinaryOperator>();
    else if(v.is<iscore::UnaryOperator>())
        dbg << v.get<iscore::UnaryOperator>();

    return dbg;
}

QDebug operator<<(QDebug dbg, const iscore::Expression& v)
{
    dbg << "(";
    dbg << static_cast<const iscore::ExprData&>(v);
    for(auto& child : v.children())
    {
        dbg << *child;
    }
    dbg << ")";
    return dbg;
}


void test_parse_expr_full()
{
    for (auto& input : std::list<std::string> {
         "(dev:/minuit != [1, 2, 3.12, 'c']) and not (a:/b >= c:/d/e/f);"
})
    {
        auto f(std::begin(input)), l(std::end(input));
        Expression_parser<decltype(f)> p;

        try
        {
            expr result;
            bool ok = qi::phrase_parse(f,l,p > ';',qi::space,result);

            if (!ok)
            {
                qDebug() << "invalid input\n";
                return;
            }

            iscore::Expression e;

            Expression_builder bldr{&e};
            boost::apply_visitor(bldr, result);
            qDebug() << e;

        }
        catch (const qi::expectation_failure<decltype(f)>& e)
        {
            //std::cerr << "expectation_failure at '" << std::string(e.first, e.last) << "'\n";
        }

        //if (f!=l) std::cerr << "unparsed: '" << std::string(f,l) << "'\n";
    }

    //return 0;
}

*/



TreeNode<ExprData>::TreeNode():
    ExprData{}
{

}


TreeNode<ExprData>::TreeNode(const iscore::ExprData& data):
    ExprData{data}
{

}


TreeNode<ExprData>::TreeNode(iscore::ExprData&& data):
    ExprData{std::move(data)}
{

}


TreeNode<ExprData>::TreeNode(const TreeNode<iscore::ExprData>& source, TreeNode<iscore::ExprData>* parent):
    ExprData{static_cast<const ExprData&>(source)},
    m_parent{parent}
{
    for(const auto& child : source.children())
    {
        this->addChild(new TreeNode<ExprData>{*child, this});
    }
}


TreeNode<ExprData>&TreeNode<ExprData>::operator=(const TreeNode<iscore::ExprData>& source)
{
    static_cast<ExprData&>(*this) = static_cast<const ExprData&>(source);

    qDeleteAll(m_children);
    for(const auto& child : source.children())
    {
        this->addChild(new TreeNode<ExprData>{*child, this});
    }

    return *this;
}


TreeNode<ExprData>::~TreeNode()
{
    qDeleteAll(m_children);
}


void TreeNode<ExprData>::setParent(TreeNode<iscore::ExprData>* parent)
{
    ISCORE_ASSERT(!parent->is<iscore::Relation>());
    if(m_parent)
        m_parent->removeChild(this);

    m_parent = parent;
    m_parent->addChild(this);
}


TreeNode<iscore::ExprData>*TreeNode<ExprData>::parent() const
{
    return m_parent;
}


void TreeNode<ExprData>::insertChild(int index, TreeNode<iscore::ExprData>* n)
{
    ISCORE_ASSERT(n);
    n->m_parent = this;
    m_children.insert(index, n);
}


void TreeNode<ExprData>::addChild(TreeNode<iscore::ExprData>* n)
{
    ISCORE_ASSERT(n);

    ISCORE_ASSERT(!this->is<iscore::Relation>());
    n->m_parent = this;
    m_children.append(n);
}


void TreeNode<ExprData>::swapChildren(int oldIndex, int newIndex)
{
    ISCORE_ASSERT(oldIndex < m_children.count());
    ISCORE_ASSERT(newIndex < m_children.count());

    m_children.swap(oldIndex, newIndex);
}


TreeNode<iscore::ExprData>*TreeNode<ExprData>::takeChild(int index)
{
    TreeNode* n = m_children.takeAt(index);
    ISCORE_ASSERT(n);
    n->m_parent = 0;
    return n;
}


void TreeNode<ExprData>::removeChild(TreeNode<iscore::ExprData>* child)
{
    m_children.removeAll(child);
}




QString ExprData::toString() const
{
    static const QMap<iscore::BinaryOperator, QString> binopMap{
      { iscore::BinaryOperator::And, "and" },
      { iscore::BinaryOperator::Or, "or" },
      { iscore::BinaryOperator::Xor, "xor" },
    };
    static const QMap<iscore::UnaryOperator, QString> unopMap{
        { iscore::UnaryOperator::Not, "not" },
    };

    if(is<iscore::Relation>())
        return get<iscore::Relation>().toString();
    else if(is<iscore::BinaryOperator>())
         return binopMap[(get<iscore::BinaryOperator>())];
    else if(is<iscore::UnaryOperator>())
        return unopMap[(get<iscore::UnaryOperator>())];
    else
        ISCORE_ABORT;
}

QString TreeNode<ExprData>::toString() const
{
    QString s;

    auto exprstr = static_cast<const iscore::ExprData&>(*this).toString();
    if(m_children.count() == 0) // Relation
    {
        s = "(" + exprstr + ")";
    }
    else if(m_children.count() == 1) // unop
    {
        s = "(" + exprstr + m_children.at(0)->toString() + ")";
    }
    else // binop
    {
        int n = 0;
        int max_n = m_children.count() - 1;
        for(const auto& child : m_children)
        {
            s += child->toString();
            if(n < max_n)
            {
                s += exprstr;
                n++;
            }
        }
    }

    return s;
}
