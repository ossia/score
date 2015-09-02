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

// Inspired from stackoverflow answer :
// http://stackoverflow.com/a/8707598/1495627
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/variant/recursive_wrapper.hpp>
#include <boost/fusion/adapted.hpp>
namespace {
namespace qi    = boost::spirit::qi;
namespace phx   = boost::phoenix;

struct op_or  {};
struct op_and {};
struct op_xor {};
struct op_not {};

typedef std::string var;
template <typename tag> struct binop;
template <typename tag> struct unop;

typedef boost::variant<var,
boost::recursive_wrapper<unop <op_not> >,
boost::recursive_wrapper<binop<op_and> >,
boost::recursive_wrapper<binop<op_xor> >,
boost::recursive_wrapper<binop<op_or> >
> expr;

template <typename tag> struct binop
{
        explicit binop(const expr& l, const expr& r) : oper1(l), oper2(r) { }
        expr oper1, oper2;
};

template <typename tag> struct unop
{
        explicit unop(const expr& o) : oper1(o) { }
        expr oper1;
};

struct printer : boost::static_visitor<void>
{
        printer(std::ostream& os) : _os(os) {}
        std::ostream& _os;

        //
        void operator()(const var& v) const { _os << v; }

        void operator()(const binop<op_and>& b) const { print(" & ", b.oper1, b.oper2); }
        void operator()(const binop<op_or >& b) const { print(" | ", b.oper1, b.oper2); }
        void operator()(const binop<op_xor>& b) const { print(" ^ ", b.oper1, b.oper2); }

        void print(const std::string& op, const expr& l, const expr& r) const
        {
            _os << "(";
            boost::apply_visitor(*this, l);
            _os << op;
            boost::apply_visitor(*this, r);
            _os << ")";
        }

        void operator()(const unop<op_not>& u) const
        {
            _os << "(";
            _os << "!";
            boost::apply_visitor(*this, u.oper1);
            _os << ")";
        }
};

std::ostream& operator<<(std::ostream& os, const expr& e)
{
    boost::apply_visitor(printer(os), e); return os;
}

template <typename It, typename Skipper = qi::space_type>
struct parser : qi::grammar<It, expr(), Skipper>
{
    parser() : parser::base_type(expr_)
    {
        using namespace qi;

        expr_  = or_.alias();

        or_  = (xor_ >> "or"  >> or_ ) [ _val = phx::construct<binop<op_or >>(_1, _2) ] | xor_   [ _val = _1 ];
        xor_ = (and_ >> "xor" >> xor_) [ _val = phx::construct<binop<op_xor>>(_1, _2) ] | and_   [ _val = _1 ];
        and_ = (not_ >> "and" >> and_) [ _val = phx::construct<binop<op_and>>(_1, _2) ] | not_   [ _val = _1 ];
        not_ = ("not" > simple       ) [ _val = phx::construct<unop <op_not>>(_1)     ] | simple [ _val = _1 ];

        simple = (('(' > expr_ > ')') | var_);
        var_ = qi::lexeme[ +alpha ] | ':' | '/' ;

        BOOST_SPIRIT_DEBUG_NODE(expr_);
        BOOST_SPIRIT_DEBUG_NODE(or_);
        BOOST_SPIRIT_DEBUG_NODE(xor_);
        BOOST_SPIRIT_DEBUG_NODE(and_);
        BOOST_SPIRIT_DEBUG_NODE(not_);
        BOOST_SPIRIT_DEBUG_NODE(simple);
        BOOST_SPIRIT_DEBUG_NODE(var_);
    }

    private:
    qi::rule<It, var() , Skipper> var_;
    qi::rule<It, expr(), Skipper> not_, and_, xor_, or_, simple, expr_;
};

int test()
{
    for (auto& input : std::list<std::string> {
         // From the OP:
         "(a and b) xor ((c and d) or (a and b));",
         "a and b xor (c and d or a and b);",

         /// Simpler tests:
         "a and b;",
         "a or b;",
         "a xor b;",
         "not a;",
         "not a and b;",
         "not (a and b);",
         "a or b or c;",
})
    {
        auto f(std::begin(input)), l(std::end(input));
        parser<decltype(f)> p;

        try
        {
            expr result;
            bool ok = qi::phrase_parse(f,l,p > ';',qi::space,result);

            if (!ok)
                std::cerr << "invalid input\n";
            else
                std::cout << "result: " << result << "\n";

        } catch (const qi::expectation_failure<decltype(f)>& e)
        {
            std::cerr << "expectation_failure at '" << std::string(e.first, e.last) << "'\n";
        }

        if (f!=l) std::cerr << "unparsed: '" << std::string(f,l) << "'\n";
    }

    return 0;
}

}


// Maps to GiNaC::relational::operators
static const QStringList operator_map_rels{"==", "!=", ">", "<", ">=", "<="}; // In the order of iscore::Relation::Operator
static const QStringList ordered_rels{"==", "!=", "<=", ">=", "<", ">"}; // Else parsing fails due to < matching before <=

static iscore::Relation::Operator toOp(const QString& str)
{
    return static_cast<iscore::Relation::Operator>(operator_map_rels.indexOf(str));
}

static iscore::RelationMember toRelationMember(QString str, bool* ok)
{
    // If it's matching an address, the convert it to an address
    if(iscore::Address::validateString(str))
    {
        return iscore::Address::fromString(str);
    }
    else
    {
        // Else try to convert it to a value.
        bool local_ok = false;
        iscore::Value v;
        v.val = str.toInt(&local_ok);
        if(local_ok)
        {
            return v;
        }

        v.val = str.toFloat(&local_ok);
        if(local_ok)
        {
            return v;
        }

        if(str == "true")
        {
            v.val = true;
            return v;
        }

        if(str == "false")
        {
            v.val = false;
            return v;
        }

        if(str.size() == 3 && str.at(0) == '\'' && str.at(2) ==  '\'')
        {
            // char ('a').
            v.val = str.at(1);
            return v;
        }

        if(str.at(0) == '"' && str.at(str.size() - 1) == '"')
        {
            // string ("dada dodo")
            str.remove(0, 1);
            str.remove(str.size() - 1, 1);

            v.val = str;
            return v;
        }

        *ok = false;
        return v;


    }
}

static iscore::Relation toRelation(const QString& str, bool* ok)
{
    qDebug() << str;

    QString found_rel;
    QStringList res;
    for(const QString& rel : ordered_rels)
    {
        if(str.contains(rel))
        {
            res = str.split(rel);
            found_rel = rel;
            break;
        }
    }

    if(res.size() != 2)
    {
        *ok = false;
        return {};
    }


    iscore::Relation relation;

    relation.op = toOp(found_rel);
    relation.lhs = toRelationMember(res[0], ok);
    relation.rhs = toRelationMember(res[1], ok);

    *ok = true;
    return relation;
}

struct expr_builder : boost::static_visitor<void>
{
        expr_builder(iscore::Expression& rt):
            root{rt}
        {

        }

        iscore::Expression& root;
        iscore::Expression* current{};

        void operator()(const var& v) const
        {
            auto str = QString::fromStdString(v);
            ISCORE_ASSERT(current);

            // This should be a relation.

            bool b = false;
            auto rel = toRelation(str, &b);
            if(b)
            {
                current->addChild(new iscore::Expression(rel, current));
            }
            else
            {
                throw std::runtime_error("Invalid input");
            }
        }

        template<typename BinOp>
        void rec(const BinOp& b, iscore::BoolOperator op)
        {
            auto prev_current = current;
            auto expr = new iscore::Expression(op, current);
            current->addChild(expr);
            current = expr;

            boost::apply_visitor(*this, b.oper1);
            boost::apply_visitor(*this, b.oper2);

            current = prev_current;
        }

        void operator()(const binop<op_and>& b) {
            rec(b, iscore::BoolOperator::And);
        }
        void operator()(const binop<op_or >& b) {
            rec(b, iscore::BoolOperator::Or);
        }
        void operator()(const binop<op_xor>& b) {
            rec(b, iscore::BoolOperator::Xor);
        }

        void operator()(const unop<op_not>& u)
        {
            //_os << "(";
            //_os << "!";
            boost::apply_visitor(*this, u.oper1);
            //_os << ")";
        }
};


iscore::Expression iscore::parse(const QString& str)
{
    auto input = str.toStdString();
    auto f(std::begin(input)), l(std::end(input));
    parser<decltype(f)> p;

    try
    {
        expr result;
        bool ok = qi::phrase_parse(f, l, p > ';', qi::space, result);

        if (!ok)
            std::cerr << "invalid input\n";
        else
        {

            std::cout << "result: " << result << "\n";
            iscore::Expression e;

            expr_builder bldr{e};
            boost::apply_visitor(bldr, result);
            return e;
        }

    } catch (const qi::expectation_failure<decltype(f)>& e)
    {
        std::cerr << "expectation_failure at '" << std::string(e.first, e.last) << "'\n";
    }

    if (f!=l) std::cerr << "unparsed: '" << std::string(f,l) << "'\n";

    ISCORE_ABORT;
    return {};
}

// Taken from boost doc

namespace boost { namespace spirit { namespace traits
{
    // Make Qi recognize QString as a container
    template <> struct is_container<QString> : mpl::true_ {};

    // Expose the container's (QString's) value_type
    template <> struct container_value<QString> : mpl::identity<QChar> {};

    // Define how to insert a new element at the end of the container (QString)
    template <>
    struct push_back_container<QString, QChar>
    {
        static bool call(QString& c, QChar const& val)
        {
            c.append(val);
            return true;
        }
    };

    // Test if a QString is empty (required for debug)
    template <>
    struct is_empty_container<QString>
    {
        static bool call(QString const& c)
        {
            return c.isEmpty();
        }
    };

    // Define how to stream a QString (required for debug)
    template <typename Out, typename Enable>
    struct print_attribute_debug<Out, QString, Enable>
    {
        static void call(Out& out, QString const& val)
        {
            out << val.toStdString();
        }
    };
}}}

///////////////////////////////////////////////////////////////////////////////
namespace client
{
    template <typename Iterator>
    bool parse_qstring(Iterator first, Iterator last, QString& t)
    {
        using boost::spirit::qi::char_;
        using boost::spirit::ascii::space;
        using boost::spirit::qi::phrase_parse;

        bool r = phrase_parse(first, last, +char_, space, t);
        if (!r || first != last) // fail if we did not get a full match
            return false;

        return r;
    }

    class Address
    {
            std::string device;
            std::vector<std::string> path;
    };


    struct employee
    {
        int age;
        std::string surname;
        std::string forename;
        double salary;
    };

}using namespace boost::fusion;

using string_type = QString;
template<typename T> using vec_type = QList<T>;
struct StdAddress
{
        string_type device;
        vec_type<string_type> path;
};

BOOST_FUSION_ADAPT_STRUCT(
    iscore::Address,
            (string_type, device)
    (QStringList, path)
)
using namespace boost::phoenix;
using boost::spirit::qi::rule;
template <typename Iterator>
struct address_parser : qi::grammar<Iterator, iscore::Address()>
{
    address_parser() : address_parser::base_type(start)
    {
        using qi::alnum;

        dev = +alnum;
        member_elt = +alnum;
        path %= (
                    +("/" >> member_elt)
                  | "/"
                );
        start %= dev >> ":" >> path;

    }

    qi::rule<Iterator, string_type()> dev;
    qi::rule<Iterator, string_type()> member_elt;
    qi::rule<Iterator, vec_type<string_type>()> path;
    qi::rule<Iterator, iscore::Address()> start;
};
#include <boost/spirit/include/qi_real.hpp>

BOOST_FUSION_ADAPT_STRUCT(
    iscore::Value,
            (QVariant, val)
)
        template <typename Iterator>
        struct value_parser : qi::grammar<Iterator, iscore::Value()>
        {
            value_parser() : value_parser::base_type(start)
            {
                using qi::alnum;
                using boost::spirit::qi::skip;
                using boost::spirit::int_;
                using boost::spirit::qi::real_parser;
                using boost::spirit::qi::char_;

                char_parser %= "'" >> (char_ - "'") >> "'";
                str_parser %= '"' >> qi::lexeme [ +(char_ - '"') ] >> '"';

                //tuple_parser %= "[" >> ("," % variant) >> "]";
                tuple_parser %= skip(boost::spirit::ascii::space) [ "[" >> (variant % ",") >> "]" ];
                variant %=  real_parser<float, boost::spirit::qi::strict_real_policies<float> >()
                        | int_
                        | char_parser
                        | str_parser
                        | tuple_parser;

                start %= variant;

            }

            qi::rule<Iterator, QVariantList()> tuple_parser;
            qi::rule<Iterator, QChar()> char_parser;
            qi::rule<Iterator, string_type()> str_parser;
            qi::rule<Iterator, QVariant()> variant;
            qi::rule<Iterator, iscore::Value()> start;
        };



void lalala()
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

template <typename Iterator>
struct relation_parser : qi::grammar<Iterator, iscore::Relation()>
{

};
/*

template <typename Iterator>
struct rel_pareser : qi::grammar<Iterator, unsigned()>
{
    rel_pareser() : rel_pareser::base_type(start)
    {
        using qi::eps;
        using qi::lit;
        using qi::_val;
        using qi::_1;
        using ascii::char_;


        start = eps             [_val = 0] >>
            (
                +lit('M')       [_val += 1000]
                ||  hundreds    [_val += _1]
                ||  tens        [_val += _1]
                ||  ones        [_val += _1]
            )
        ;

    }

    qi::rule<Iterator, unsigned()> start;
    qi::rule<std::string::iterator, qi::space> addr;

};
*/
void expr_parse_test()
{
    test_parse_value();
    lalala();
    iscore::parse("minuit:/device/lol <= 123;");
    iscore::parse("minuit:/device/lol < 123;");
    iscore::parse("minuit:/device/lol != 123.45;");
    iscore::parse("minuit:/device/lol == 'h';");
    iscore::parse("minuit:/device/lol != \"dada dodo\";");
}
