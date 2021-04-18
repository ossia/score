#pragma once
/*! \page Contexts
 *
 * Contexts are a solution to the problem of requiring global state.
 * There are multiple nested levels of context in score :
 *
 * * score::ApplicationContext : where classes are registered
 * * score::GUIApplicationContext : extends ApplicationContext, available in
 * the GUI software (not in the command-line & embedded players)
 * * score::DocumentContext
 * * Scenario::Context
 * * Various execution contexts, for audio, etc.
 *
 * A context is simply a class that references other classes useful in a given
 * context.
 *
 * For instance, score::ApplicationContext gives access to the factories, etc.
 * score::DocumentContext gives access to the command and selection stack of a
 * given document, as well as the model object tree.
 *
 * Ideally, contexts should not be called through functions but passed from
 * parent to child, to respect dependency injection and minimize the number of
 * global function calls.
 *
 * Some common classes will have a built-in reference to the ApplicationContext
 * : for instance all commands, and all serialization classes
 */
