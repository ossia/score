#pragma once

/*! \mainpage
 *
 * Welcome to the i-score code documentation.
 * <br><br>
 * Here is the documentation of core concepts used throughout the i-score code base.
 * <br>
 * All the following concepts are sowewhat interdependents, hence reading everything twice
 * may be useful to get a clear mental picture.
 *<br>
 * * \ref Models
 * * \ref PluginsFactoriesAndInterfaces
 * * \ref Contexts
 * * \ref Commands
 * * \ref Serialization
 * * \ref Documents
 * * \ref Actions
 * * \ref CodingStyle
 * * \ref BuildSystem
 * * \ref Metadata
 * * \ref ModelViewPresenter
 *<br><br>
 * Documentation of specific plug-ins :
 * <br>
 * * \ref Scenario
 * * \ref Curve
 * * \ref Automation
 * * \ref Device
 * * \ref State
 * * \ref Process
 * * \ref Explorer
 *<br>
 * To contribute to i-score, it can also be useful to have a look at the
 * tutorial plug-in : https://github.com/OSSIA/iscore-addon-tutorial
 */

/*! \namespace iscore
 * \brief Base toolkit upon which the software is built.
 *
 * This namespace contains only non-domain specific classes
 * and utilities : serialization, model-view, documents, etc.
 *
 * It is split in two folders :
 *
 * * `core` is the internal mechanic to set-up the software : the actual widget classes,
 *   the plug-in loading code, etc.
 * * `iscore` is the "public" part of the i-score API : this code can be used by plug-ins.
 */

/*! \page CodingStyle Coding Style
 *
 * \section Philosophy General philosophy
 * API vs i-score
 *
 * \section Qt Qt versus Modern C++
 * * vector / qvector / qlist
 * * strings
 * * pointers : unique_ptr vs qt object model
 *
 * \section Inheritance
 * * Try to limit inheritance to the strict necessary for objects in the object hierarchy.
 *
 * Most of the time, there should only be a base class offered as a plug-in interface,
 * and implementations of this base class.
 *
 * * However, inheritance and multi-inheritance for non-model classes is not a problem (i.e.
 * inheritance as a tool).
 *
 * * Don't forget virtual destructors for inheritance across plug-ins,
 * else the program will fail at runtime.
 *
 * \section Values Passing values
 * * Prefer storing values if possible, not dynamically allocated objects.
 * e.g.
 *
 * \code
 * class MyClass
 * {
 *   OtherClass m_subobject;
 * };
 * \endcode
 *
 * instead of
 * \code
 * class MyClass
 * {
 *   OtherClass* m_subobject = new OtherClass;
 * };
 * \endcode
 *
 * \section Templates
 * Templates are an useful but complicated tool. They increase compilation times greatly,
 * but can provide better performance than standard inheritance for polymorphism.
 *
 * Core library elements should be templated if it can improve genericity and performance.
 * They can also be used to increase code safety, by tagging classes.
 *
 */

/*! \page PluginsFactoriesAndInterfaces Plug-ins, factories and interfaces
 *
 * Due to the plug-in architecture of the software, a system to load classes has to be devised.
 *
 * It extends Qt's plug-in system with factory registration, and works in tandem with Contexts.
 *
 * The core plug-ins are in the folder base/plugins.
 *
 * Other plug-ins are available on the github page : https://github.com/OSSIA/
 *
 * For instance:
 *
 * * iscore-addon-audio provides audio sequencer features
 * * iscore-addon-remotecontrol exposes the object tree through a WebSockets protocol
 * * etc...
 *
 * \section Anatomy Anatomy of a plug-in
 *
 * A plug-in generally has the following file structure:
 *
 * \code
 * plugin-name/
 *            /CMakeLists.txt
 *            /the_plugin.hpp
 *            /the_plugin.cpp
 *            /ThePlugin/{code of the plug-in}
 * \endcode
 *
 * For i-score to detect new plug-ins, they have to be put in the `base/addons/` folder,
 * and CMake has to be re-run.
 *
 * The root class of the plug-in is part of the Qt Plugin System (see its documentation).
 * It provides multiple initialization functions, going from the most global to the most specific :
 *
 * * Constructor
 * * ApplicationPlugin
 * * Lists of factories
 * * Factories
 * * Commands
 *
 * See for instance iscore_plugin_scenario for the most complex case, or iscore_plugin_js for a simple
 * case that only adds a process.
 *
 * The classes are then registered in the ApplicationContext; they will be accessible from the whole software.
 * Classes are strongly-typed as far as possible, and categorized by their interfaces.
 *
 * Each interface will be registered in the relevant list of interfaces; else there is no point
 * in using a plug-in system.
 *
 * For instance, let's take the UI panels, such as history, device explorer, etc.
 *
 * * The application registers a list of factories: iscore::PanelDelegateFactoryList.
 * * For each kind of panel, a factory will be registered in this list.
 * * When creating the actual panels, iscore::PanelDelegateFactoryList is iterated:
 * * Each factory's iscore::PanelDelegateFactory::make function is called and the panel is shown.
 *
 * When loading a new document, many classes will depend on plug-in interfaces for loading.
 * For instance, when loading a Process::ProcessModel, one has to find the right factory to use to create the
 * correct instance of the Process::ProcessModel.
 *
 * This is achieved by adding an unique identifier to each plug-in class, through the macros ISCORE_INTERFACE and ISCORE_CONCRETE.
 * Such identifiers can be generated through the `uuidgen` command on Linux, macOS and Windows.
 *
 *
 * \section NewClass Adding a new component
 *
 * Adding a new class that matches an exisiting interface, for instance for providing a new settings panel, is straightforward:
 *
 * * Inherit from the base classes required for this component.
 *   The classes to reimplement to provide a custom settings panel are iscore::SettingsDelegateModel, iscore::SettingsDelegatePresenter, iscore::SettingsDelegateView.
 * * Inherit from the corresponding factory : in this case iscore::SettingsDelegateFactory.
 *   In many cases, the factories are very simple code that only does `new MyImplementationOfTheClass`.
 *   Hence, to simplify the user code and minimize the amount of code, template overloads and macros are provided.
 *   In the case of the settings, one could either :
 *     * Reimplement iscore::SettingsDelegateFactory entirely
 *     * Extend SettingsDelegateFactory_T<MyModel, MyPresenter, MyView> and add ISCORE_CONCRETE(a-generated-uuid) :
 *       \code
 *       class MyFactory : public iscore::SettingsDelegateFactory_T<MyModel, MyPresenter, MyView> {
 *           ISCORE_CONCRETE("c42ff76c-85bd-42c2-9879-cdc660f968f3")
 *       };
 *       \endcode
 *     * Call the macro ISCORE_DECLARE_SETTINGS_FACTORY :
 *       \code
           ISCORE_DECLARE_SETTINGS_FACTORY(MyFactory, MyModel, MyPresenter, MyView, "c42ff76c-85bd-42c2-9879-cdc660f968f3")
 *       \endcode
 *
 * * Add them to the list of factories in the root plug-in file.
 *   That is, in the `my_plugin` file which extends `iscore::FactoryInterface_QtInterface`,
 *   add a line such as `FW<iscore::SettingsDelegateFactory, MyFactory>`.
 *   i-score will then instantiate and register `MyFactory` automatically on startup.
 *
 * \section NewInterface Declaring a new interface
 *
 * Declaring a new interface is when you want to provide a new kind of behaviour
 * that can itself be extended further through other plug-ins.
 * For instance, the audio addon provides Faust and LV2 support, through an interface
 * that allows further plug-ins to add new kind of audio effects, such as VST.
 *
 * * The first thing to do is to isolate the class or group of classes that constitute the feature.
 *   These should be standard abstract classes with virtual methods for the functions you want to override.
 *   We will take the example of the protocol implementation and use Device::DeviceInterface as a reference.
 * * Then, create the abstract factory from which concrete factories will inherit.
 *   For instance, Device::ProtocolFactory.
 *
 *   The abstract factory should:
 *    * inherit from `iscore::Interface<TheFactory>`
 *    * have the ISCORE_INTERFACE macro.
 *    * have relevant virtual functions. For instance, Device::ProtocolFactory::makeDevice.
 *
 *   These functions can be pure virtual, or provide a default dummy implementation.
 * * Then, create the factory list class.
 *   Most of the time, the only thing to do is inheriting from `iscore::InterfaceList<TheFactory>`.
 * * Like we saw in the previous section, helper templates and macros should be provided.
 * * Finally, in the root plug-in class, register the factory list in the `factoryFamilies()` function.
 *
 * Now, user code can look for registered interfaces by doing :
 *
 * \code
 * auto& ctx = iscore::AppContext();
 * auto& list = ctx.interfaces<TheFactoryList>();
 * \endcode
 *
 * This list can be iterated; it is also possible to look for a concrete class by type or UUID:
 *
 * \code
 * if(auto myFactory = list.get<MyConcreteFactory>()) { ... }
 * if(auto loadedFactory = list.get(anUuid)) {
 *     loadedFactory->makeDevice(...);
 * }
 * for(auto& factory : list) {
 *   if(factory.matches("something")) {
 *     factory.createStuff();
 *   }
 * }
 * \endcode
 *
 * \section AddonManager Add-on manager
 *
 * i-score provides a tentative add-on manager.
 *
 * Its implementation is in iscore-plugin-pluginsettings.
 * Add-ons are listed in a central repository :
 *
 * https://github.com/OSSIA/iscore-addons/blob/master/addons.json
 *
 * Each addon provides a JSON description file with the following keys :
 *
 * * One key per plug-in architecture. The currently supported formats are :
 *   * windows-x86
 *   * windows-amd64
 *   * darwin-amd64
 *   * linux-amd64
 *   * linux-arm
 *
 * The value should be a link to a zipped addon package for the relevant architecture.
 *
 *  * Metadata keys:
 *    * name
 *    * version
 *    * short: a short description text
 *    * long: a long description text, can contain HTML
 *    * small: a miniature image (64x64)
 *    * large: a large image (512x512)
 *    * key: a unique identifier for the add-on
 *
 * An add-on package should have the following layout :
 *
 * \code
 * TheAddon.zip/AddonName/localaddon.json
 * TheAddon.zip/AddonName/TheAddon-arch.{so,dylib,dll}
 * \endcode
 *
 * `localaddon.json` has the same metadata keys than before.
 * Instead of an url, the architecture key has the filename as value, e.g. "TheAddon-amd64.dylib".
 *
 * Add-ons are searched for in `$DOCUMENTS/i-score/addons`.
 *
 */

/*! \page Contexts
 *
 * Contexts are a solution to the problem of requiring global state.
 * There are multiple nested levels of context in i-score :
 *
 * * iscore::ApplicationContext : where classes are registered
 * * iscore::GuiApplicationContext : extends ApplicationContext, available in the GUI software
 *   (not in the command-line & embedded players)
 * * iscore::DocumentContext
 * * Scenario::ProcessPresenterContext
 * * Various execution contexts, for audio, etc.
 *
 * A context is simply a class that references other classes useful in a given context.
 *
 * For instance, iscore::ApplicationContext gives access to the factories, etc.
 * iscore::DocumentContext gives access to the command and selection stack of a given document,
 * as well as the model object tree.
 *
 * Ideally, contexts should not be called through functions but passed from parent to child,
 * to respect dependency injection and minimize the number of global function calls.
 *
 * Some common classes will have a built-in reference to the ApplicationContext : for instance
 * all commands, and all serialization classes
 */

/*! \page Commands
 *
 * Commands are used for undo-redo.
 * A command is simply a class which inherits from iscore::Command.
 *
 * Commands can be serialized: this allows to restore everything on a crash (see iscore::DocumentBackupManager).
 *
 * \section CreatingCommand Creating a command
 *
 * * Inherit from iscore::Command
 * * Add the required data members that will allow to perform undo and redo.
 *   These should be most of the time:
 *    * The Path to the model object that will be changed by the command.
 *    * The old value and the new value
 * * Add a relevant constructor. Most of the time it will take a reference to the changed object, and the new value, since
 *   the old one can be queried from this object.
 * * Reimplement iscore::Command::undo() and iscore::Command::redo()
 * * Reimplement the serialization methods.
 * * Add the ISCORE_COMMAND_DECL macro with the relevant metadata for the command.
 *
 * Some examples of commands that can be used as base :
 * * Scenario::Command::SetCommentText for a very simple command that only changes a text
 * * Scenario::Command::AddOnlyProcessToConstraint for a command that leverages interfaces and creates new elements in a score
 *
 *
 * \section LaunchingCommands Launching commands
 *
 * Launching a command requires access to the command stack (see iscore::CommandStack).
 * A reference to the current document's command stack is available in `iscore::DocumentContext`.
 *
 * In the simplest case, sending a command would look like :
 *
 * \code
 * CommandDispatcher<> dispatcher{ctx.commandStack};
 * dispatcher.submitCommand<TheCommand>(commandArg1, commandArg1, ...);
 * \endcode
 *
 * See for instance Scenario::EventPresenter : a dispatcher is stored as a class member.
 * On a drag'n'drop, a command is sent.
 *
 * \section SpecialCommands Special commands
 *
 * To simplify some common use cases, the following
 *
 * \subsection Dispatchers Command dispatchers
 *
 * Sometimes just sending a command is not good enough.
 *
 *
 *
 *
 */

/*! \page Serialization
 *
 */

/*! \page Documents
 *
 */

/*! \page Metadata
 *
 * Static, dynamic, Qt...
 */

/*! \page Actions
 *
 */

/*! \page BuildSystem Build system
 *
 */

/*! \page ModelViewPresenter Model-View-Presenter separation
 *
 */
