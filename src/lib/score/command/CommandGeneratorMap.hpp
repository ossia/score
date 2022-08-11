#pragma once
#include <score/command/CommandFactoryKey.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/std/HashMap.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QByteArray>

#include <score_lib_base_export.h>

namespace score
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
 * Reinstantiation is done through \ref
 * score::ApplicationContext::instantiateUndoCommand.
 *
 * OPTIMIZEME: right now the names are used, it should be nice to migrate
 * towards uids.
 * OPTIMIZEME : maybe we could just concatenate both keys and have a single
 * hash_map...
 */

/**
 * @brief Base factory for commands
 *
 * Base typedef for command instantiation. Allows a polymorphic use.
 */
using CommandFactory = score::Command* (*)(const QByteArray&);

/**
 * @brief The CommandGeneratorMap struct
 *
 * A map between command names and corresponding factories.
 */
using CommandGeneratorMap = std::vector<std::pair<CommandKey, CommandFactory>>;

namespace score
{
namespace commands
{

/**
 * @brief Creates and inserts a new factory class for a given command, in a
 * list of such factories.
 */
struct FactoryInserter
{
  CommandGeneratorMap& fact;
  template <typename TheCommand>
  void operator()() const
  {
    SCORE_ASSERT(bool(
        ossia::find_if(fact, [](auto& e) { return e.first == TheCommand::static_key(); })
        == fact.end()));
    constexpr CommandFactory f = [](const QByteArray& data) -> score::Command* {
      auto t = new TheCommand;
      t->deserialize(data);
      return t;
    };
    fact.emplace_back(TheCommand::static_key(), f);
  }
};
}
}
