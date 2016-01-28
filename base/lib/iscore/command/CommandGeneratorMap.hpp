#pragma once
#include <QByteArray>
#include <memory>
#include <unordered_map>
#include <utility>

#include <iscore/command/CommandFactoryKey.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
class SerializableCommand;
}
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
class ISCORE_LIB_BASE_EXPORT CommandFactory
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

namespace iscore
{
namespace commands
{

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
