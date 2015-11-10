#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <QString>

#include <boost/mpl/list.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/for_each.hpp>

#include <unordered_map>
#include <memory>
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
using CommandGeneratorMap = std::unordered_map<std::string, std::unique_ptr<CommandFactory>>;

/**
 * @brief The CommandGeneratorMapInserter struct
 *
 * A functor that will instantiate all the command factories when passed to
 * a boost::mpl::for_each.
 */

struct CommandGeneratorMapInserter
{
        CommandGeneratorMap& fact;
        template< typename TheCommand > void operator()(boost::type<TheCommand>)
        {
            fact.insert(std::pair<const std::string, std::unique_ptr<CommandFactory>>{
                                TheCommand::commandName(),
                                std::unique_ptr<CommandFactory>(new GenericCommandFactory<TheCommand>)
                        }
            );
        }
};
