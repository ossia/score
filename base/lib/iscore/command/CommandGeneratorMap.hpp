#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <boost/mpl/list.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/for_each.hpp>

#include <unordered_map>
/**
 * This file contains utility classes to instantiate commands
 * when they are received from the network.
 *
 * Examples to use it can be seen in the curve plugin's AutomationControl (small)
 * and in ScenarioControl (big).
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
class CommandGenerator
{
    public:
        virtual ~CommandGenerator() = default;
        virtual iscore::SerializableCommand* operator()() const = 0;
};

/**
 * @brief The CommandGeneratorImpl class
 *
 * Factory that covers the standard use case for command instantiation.
 */
template<typename T>
class CommandGeneratorImpl : public CommandGenerator
{
    public:
        iscore::SerializableCommand* operator()() const override
        {
            return new T;
        }
};

/**
 * @brief The CommandGeneratorMap struct
 *
 * A map between command names and corresponding factories.
 */
using CommandGeneratorMap = std::map<QString, std::unique_ptr<CommandGenerator>>;

/**
 * @brief The CommandGeneratorMapInserter struct
 *
 * A functor that will instantiate all the command factories when passed to
 * a boost::mpl::for_each.
 *
 * The CommandFactory is a singleton.
 */
template<typename CommandFactory>
struct CommandGeneratorMapInserter
{
        template< typename TheCommand > void operator()(boost::type<TheCommand>)
        {
            CommandFactory::map.insert(
                        std::move(
                            std::pair<QString, std::unique_ptr<CommandGenerator>>{
                                TheCommand::commandName(),
                                std::unique_ptr<CommandGenerator>(new CommandGeneratorImpl<TheCommand>)
                            }
                        )
            );
        }
};
