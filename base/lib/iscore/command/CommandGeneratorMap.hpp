#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <unordered_map>
#include <memory>
/**
 * This file contains utility classes to instantiate commands
 * when they are received from the network.
 *
 * Examples to use it can be seen in the curve plugin's AutomationApplicationPlugin (small)
 * and in ScenarioApplicationPlugin (big).
 *
 * It involves putting the list of the command classes in a boost::mpl::list.
 * For each command, a factory will be generated and stored in a map with
 * the command name, to allow fast instantiation upon command reception.
 *
 * OPTIMIZEME: right now the names are used, it should be nice to migrate towards uids.
 */

/**
 * @brief The CommandGenerator class
 *
 * Base class for command instantiation. Allows a polymorphic use.
 */
class CommandFactory
{
    public:
        virtual ~CommandFactory();
        virtual iscore::SerializableCommand* operator()(const QByteArray& data) const = 0;
};

/**
 * @brief The CommandGeneratorImpl class
 *
 * Factory that covers the standard use case for command instantiation.
 */
template<typename T>
class GenericCommandFactory : public CommandFactory
{
    public:
        iscore::SerializableCommand* operator()(const QByteArray& data) const override
        {
            auto t = new T;
            t->deserialize(data);
            return t;
        }
};

/**
 * @brief The CommandGeneratorMap struct
 *
 * A map between command names and corresponding factories.
 */
using CommandGeneratorMap = std::unordered_map<CommandFactoryKey, std::unique_ptr<CommandFactory>>;


// Courtesy of Daniel J-H
// https://gist.github.com/daniel-j-h
namespace iscore
{
namespace commands
{
template <typename... Args> struct TypeList { };

namespace detail {

// Forward declare, in order to pattern match via specialization
template <typename ...> struct TypeVisitor;

// Pattern match to extract the TypeLists types into Args
template <template <typename ...> class Sequence, typename... Args>
struct TypeVisitor<Sequence<Args...>> {

  template <typename F>
  static constexpr void visit(const F& f) {
    // Start recursive visitation for each type
    do_visit<F, Args...>(f);
  }

  // Allow empty sequence
  template <typename F>
  static constexpr void do_visit(const F& f) { }

  // Base case: one type, invoke functor
  template <typename F, typename Head>
  static constexpr void do_visit(const F& f) {
    f.template visit<Head>();
  }

  // At least [Head, Next], Tail... can be empty
  template <typename F, typename Head, typename Next, typename... Tail>
  static constexpr void do_visit(const F& f) {
    // visit head and recurse visitation on rest
    do_visit<F, Head>(f), do_visit<F, Next, Tail...>(f);
  }

};
} // End Detail
// Invokes the functor with every type, this code generation is done at compile time
template <typename Sequence, typename F>
constexpr void ForEach(const F& f)
{
    detail::TypeVisitor<Sequence>::template visit<F>(f);
}


struct FactoryInserter
{
        CommandGeneratorMap& fact;
        template< typename TheCommand > void visit() const
        {
            fact.insert(std::pair<const CommandFactoryKey, std::unique_ptr<CommandFactory>>{
                                TheCommand::static_key(),
                                std::unique_ptr<CommandFactory>(new GenericCommandFactory<TheCommand>)
                        });
        }
};

}
}
