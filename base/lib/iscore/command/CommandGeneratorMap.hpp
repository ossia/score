#pragma once
#include <QByteArray>
#include <iscore/tools/std/HashMap.hpp>
#include <memory>
#include <utility>

#include <iscore/command/CommandFactoryKey.hpp>
#include <iscore_lib_base_export.h>

namespace iscore
{
class Command;
}
/**
 * \file CommandGeneratorMap.hpp
 *
 * This file contains utility classes to instantiate commands
 * when they are received from the network, or reloaded after a crash.
 *
 * For each command, a factory will be generated and stored in a map with
 * the command name, to allow fast instantiation upon command reception.
 *
 * Each command has two identifiers :
 * * One is the \ref CommandGroupKey used to differentiate groups of commands
 * * One is the \ref CommandKey used to differentiate commands in a same group
 *
 * They are stored in a \ref CommandData upon serialization, along with
 * the command-specific data.
 *
 * Reinstatiation is donne through \ref iscore::ApplicationContext::instantiateUndoCommand.
 *
 * OPTIMIZEME: right now the names are used, it should be nice to migrate
 * towards uids.
 * OPTIMIZEME : maybe we could just concatenate both keys and have a single hash_map...
 */

/**
 * @brief Base factory for commands
 *
 * Base class for command instantiation. Allows a polymorphic use.
 */
class ISCORE_LIB_BASE_EXPORT CommandFactory
{
public:
  virtual ~CommandFactory();
  virtual iscore::Command*
  operator()(const QByteArray& data) const = 0;
};

/**
 * @brief Standard implementation of the command factory.
 */
template <typename T>
class GenericCommandFactory : public CommandFactory
{
public:
  iscore::Command*
  operator()(const QByteArray& data) const override
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
using CommandGeneratorMap
    = iscore::hash_map<CommandKey, CommandFactory*>;

namespace iscore
{
namespace commands
{

/**
 * @brief Creates and inserts a new factory class for a given command, in a list of such factories.
 */
struct FactoryInserter
{
  CommandGeneratorMap& fact;
  template <typename TheCommand>
  void perform() const
  {
    fact.insert(
        std::pair<const CommandKey, CommandFactory*>{
            TheCommand::static_key(), new GenericCommandFactory<TheCommand>});
  }
};
}
}
