#include "Expression.hpp"
#include <algorithm>
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

bool iscore::validate(const iscore::Expression& expr)
{
    // Check that all the leaves are relations.
    return checkLeaves(&expr);
}

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

/*
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
*/

