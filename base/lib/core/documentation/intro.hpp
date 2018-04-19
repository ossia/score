#pragma once

/*! \mainpage
 *
 * Welcome to the score code documentation.
 * <br><br>
 * Here is the documentation of core concepts used throughout the score code
 *base. <br> All the following concepts are sowewhat interdependents, hence
 *reading everything twice may be useful to get a clear mental picture. <br>
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
 * To contribute to score, it can also be useful to have a look at the
 * tutorial plug-in : https://github.com/OSSIA/score-addon-tutorial
 */

/*! \namespace score
 * \brief Base toolkit upon which the software is built.
 *
 * This namespace contains only non-domain specific classes
 * and utilities : serialization, model-view, documents, etc.
 *
 * It is split in two folders :
 *
 * * `core` is the internal mechanic to set-up the software : the actual widget
 * classes, the plug-in loading code, etc.
 * * `score` is the "public" part of the score API : this code can be used by
 * plug-ins.
 */

/*! \page CodingStyle Coding Style
 *
 * \section Philosophy General philosophy
 * API vs score
 *
 * \section Qt Qt versus Modern C++
 * * vector / qvector / qlist
 * * strings
 * * pointers : unique_ptr vs qt object model
 *
 * \section Inheritance
 * * Try to limit inheritance to the strict necessary for objects in the object
 * hierarchy.
 *
 * Most of the time, there should only be a base class offered as a plug-in
 * interface, and implementations of this base class.
 *
 * * However, inheritance and multi-inheritance for non-model classes is not a
 * problem (i.e. inheritance as a tool).
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
 * Templates are an useful but complicated tool. They increase compilation
 * times greatly, but can provide better performance than standard inheritance
 * for polymorphism.
 *
 * Core library elements should be templated if it can improve genericity and
 * performance. They can also be used to increase code safety, by tagging
 * classes.
 *
 */

/*! \page PluginsFactoriesAndInterfaces Plug-ins, factories and interfaces
 *
 * Due to the plug-in architecture of the software, a system to load classes
 has to be devised.
 *
 * It extends Qt's plug-in system with factory registration, and works in
 tandem with Contexts.
 *
 * The core plug-ins are in the folder base/plugins.
 *
 * Other plug-ins are available on the github page : https://github.com/OSSIA/
 *
 * For instance:
 *
 * * score-addon-audio provides audio sequencer features
 * * score-addon-remotecontrol exposes the object tree through a WebSockets
 protocol
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
 * For score to detect new plug-ins, they have to be put in the `base/addons/`
 folder,
 * and CMake has to be re-run.
 *
 * The root class of the plug-in is part of the Qt Plugin System (see its
 documentation).
 * It provides multiple initialization functions, going from the most global to
 the most specific :
 *
 * * Constructor
 * * ApplicationPlugin
 * * Lists of factories
 * * Factories
 * * Commands
 *
 * See for instance score_plugin_scenario for the most complex case, or
 score_plugin_js for a simple
 * case that only adds a process.
 *
 * The classes are then registered in the ApplicationContext; they will be
 accessible from the whole software.
 * Classes are strongly-typed as far as possible, and categorized by their
 interfaces.
 *
 * Each interface will be registered in the relevant list of interfaces; else
 there is no point
 * in using a plug-in system.
 *
 * For instance, let's take the UI panels, such as history, device explorer,
 etc.
 *
 * * The application registers a list of factories:
 score::PanelDelegateFactoryList.
 * * For each kind of panel, a factory will be registered in this list.
 * * When creating the actual panels, score::PanelDelegateFactoryList is
 iterated:
 * * Each factory's score::PanelDelegateFactory::make function is called and
 the panel is shown.
 *
 * When loading a new document, many classes will depend on plug-in interfaces
 for loading.
 * For instance, when loading a Process::ProcessModel, one has to find the
 right factory to use to create the
 * correct instance of the Process::ProcessModel.
 *
 * This is achieved by adding an unique identifier to each plug-in class,
 through the macros SCORE_INTERFACE and SCORE_CONCRETE.
 * Such identifiers can be generated through the `uuidgen` command on Linux,
 macOS and Windows.
 *
 *
 * \section NewClass Adding a new component
 *
 * Adding a new class that matches an exisiting interface, for instance for
 providing a new settings panel, is straightforward:
 *
 * * Inherit from the base classes required for this component.
 *   The classes to reimplement to provide a custom settings panel are
 score::SettingsDelegateModel, score::SettingsDelegatePresenter,
 score::SettingsDelegateView.
 * * Inherit from the corresponding factory : in this case
 score::SettingsDelegateFactory.
 *   In many cases, the factories are very simple code that only does `new
 MyImplementationOfTheClass`.
 *   Hence, to simplify the user code and minimize the amount of code, template
 overloads and macros are provided.
 *   In the case of the settings, one could either :
 *     * Reimplement score::SettingsDelegateFactory entirely
 *     * Extend SettingsDelegateFactory_T<MyModel, MyPresenter, MyView> and add
 SCORE_CONCRETE(a-generated-uuid) :
 *       \code
 *       class MyFactory : public score::SettingsDelegateFactory_T<MyModel,
 MyPresenter, MyView> {
 *           SCORE_CONCRETE("c42ff76c-85bd-42c2-9879-cdc660f968f3")
 *       };
 *       \endcode
 *     * Call the macro SCORE_DECLARE_SETTINGS_FACTORY :
 *       \code
           SCORE_DECLARE_SETTINGS_FACTORY(MyFactory, MyModel, MyPresenter,
 MyView, "c42ff76c-85bd-42c2-9879-cdc660f968f3")
 *       \endcode
 *
 * * Add them to the list of factories in the root plug-in file.
 *   That is, in the `my_plugin` file which extends
 `score::FactoryInterface_QtInterface`,
 *   add a line such as `FW<score::SettingsDelegateFactory, MyFactory>`.
 *   score will then instantiate and register `MyFactory` automatically on
 startup.
 *
 * \section NewInterface Declaring a new interface
 *
 * Declaring a new interface is when you want to provide a new kind of
 behaviour
 * that can itself be extended further through other plug-ins.
 * For instance, the audio addon provides Faust and LV2 support, through an
 interface
 * that allows further plug-ins to add new kind of audio effects, such as VST.
 *
 * * The first thing to do is to isolate the class or group of classes that
 constitute the feature.
 *   These should be standard abstract classes with virtual methods for the
 functions you want to override.
 *   We will take the example of the protocol implementation and use
 Device::DeviceInterface as a reference.
 * * Then, create the abstract factory from which concrete factories will
 inherit.
 *   For instance, Device::ProtocolFactory.
 *
 *   The abstract factory should:
 *    * inherit from `score::Interface<TheFactory>`
 *    * have the SCORE_INTERFACE macro.
 *    * have relevant virtual functions. For instance,
 Device::ProtocolFactory::makeDevice.
 *
 *   These functions can be pure virtual, or provide a default dummy
 implementation.
 * * Then, create the factory list class.
 *   Most of the time, the only thing to do is inheriting from
 `score::InterfaceList<TheFactory>`.
 * * Like we saw in the previous section, helper templates and macros should be
 provided.
 * * Finally, in the root plug-in class, register the factory list in the
 `factoryFamilies()` function.
 *
 * Now, user code can look for registered interfaces by doing :
 *
 * \code
 * auto& ctx = score::AppContext();
 * auto& list = ctx.interfaces<TheFactoryList>();
 * \endcode
 *
 * This list can be iterated; it is also possible to look for a concrete class
 by type or UUID:
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
 * score provides a tentative add-on manager.
 *
 * Its implementation is in score-plugin-pluginsettings.
 * Add-ons are listed in a central repository :
 *
 * https://github.com/OSSIA/score-addons/blob/master/addons.json
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
 * The value should be a link to a zipped addon package for the relevant
 architecture.
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
 * Instead of an url, the architecture key has the filename as value, e.g.
 "TheAddon-amd64.dylib".
 *
 * Add-ons are searched for in `$DOCUMENTS/score/addons`.
 *
 */

/*! \page Contexts
 *
 * Contexts are a solution to the problem of requiring global state.
 * There are multiple nested levels of context in score :
 *
 * * score::ApplicationContext : where classes are registered
 * * score::GuiApplicationContext : extends ApplicationContext, available in
 * the GUI software (not in the command-line & embedded players)
 * * score::DocumentContext
 * * Scenario::ProcessPresenterContext
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

/*! \page Commands
 *
 * Commands are used for undo-redo.
 * A command is simply a class which inherits from score::Command.
 *
 * Commands can be serialized: this allows to restore everything on a crash
 * (see score::DocumentBackupManager).
 *
 * \section CreatingCommand Creating a command
 *
 * * Inherit from score::Command
 * * Add the required data members that will allow to perform undo and redo.
 *   These should be most of the time:
 *    * The Path to the model object that will be changed by the command.
 *    * The old value and the new value
 * * Add a relevant constructor. Most of the time it will take a reference to
 * the changed object, and the new value, since the old one can be queried from
 * this object.
 * * Reimplement score::Command::undo() and score::Command::redo()
 * * Reimplement the serialization methods.
 * * Add the SCORE_COMMAND_DECL macro with the relevant metadata for the
 * command. The use of this macro is mandatory: the build system scans the code
 * and generate files that will automatically register all the commands. This
 * way, when there is a crash, commands can be deserialized correctly. They can
 * also be sent through the network this way. This also means that if a command
 * is added, CMake may need to be re-run to register the command and make it
 * available to the crash restoring.
 *
 *
 * Some examples of commands that can be used as base :
 * * Scenario::Command::SetCommentText for a very simple command that only
 * changes a text
 * * Scenario::Command::AddOnlyProcessToInterval for a command that leverages
 * interfaces and creates new elements in a score
 *
 *
 * \section LaunchingCommands Launching commands
 *
 * Launching a command requires access to the command stack (see
 * score::CommandStack). A reference to the current document's command stack is
 * available in `score::DocumentContext`.
 *
 * In the simplest case, sending a command would look like :
 *
 * \code
 * CommandDispatcher<> dispatcher{ctx.commandStack};
 * dispatcher.submitCommand<TheCommand>(commandArg1, commandArg1, ...);
 * \endcode
 *
 * See for instance Scenario::EventPresenter : a dispatcher is stored as a
 * class member. On a drag'n'drop, a command is sent.
 *
 * \section SpecialCommands Special commands
 *
 * To simplify some common use cases, the following simplfified command classes
 * are available :
 *
 * * score::PropertyCommand : commands which just change a value of a member
 *                             within the Qt Property System
 * (http://doc.qt.io/qt-5/properties.html).
 *
 * * score::AggregateCommand : used when a command is made of multiple small
 * commands one after each other. For instance, the
 * Scenario::Commands::ClearSelection command is an aggregate of various
 * commands that clear various kinds of elements respectively. It should be
 * subclassed to give a meaningful name to the user.
 *
 * \subsection Dispatchers Command dispatchers
 *
 * Sometimes just sending a command is not good enough.
 * For instance, when moving a slider, we don't want to send a command each
 * time the value changes, even though it is altered in the model.
 *
 * Hence, some commands will have an additional `update()` method that takes
 * the same argument as the constructor of the command.
 *
 * The dispatchers are able to selectively create a new command the first time,
 * and update it when `submitCommand` is called again.
 * The command is then pushed in the command stack with `commit()`, or
 * abandoned with `rollback()`.
 *
 * The various useful dispatchers are :
 *
 * * MacroCommandDispatcher : appends commands to an AggregateCommand.
 * * SingleOngoingCommandDispatcher : performs the updating mechanism mentioned
 * above on a single command enforced at compile time.
 * * MultiOngoingCommandDispatcher : performs the updating mechanism on
 * multiple following commands. That is, the following code :
 *
 * \code
 * MultiOngoingCommandDispatcher disp{stack};
 * disp.submitCommand<Command1>(obj1, 0);
 * disp.submitCommand<Command1>(obj1, 1);
 * disp.submitCommand<Command1>(obj1, 2);
 * disp.submitCommand<Command2>(obj2, obj3, obj4);
 * disp.submitCommand<Command3>(obj1, 10);
 * disp.submitCommand<Command3>(obj1, 20);
 * disp.commit<MacroCommand>();
 * \endcode
 *
 * Will behind the scene do the following :
 * \code
 * auto cmd = new Command1(obj1, 0);
 * cmd->redo();
 * cmd->update(obj1, 1);
 * cmd->redo();
 * cmd->update(obj1, 2);
 * cmd->redo();
 *
 * auto cmd2 = new Command2(obj2, obj3, obj4);
 * cmd2->redo();
 *
 * auto cmd3 = new Command3(obj1, 10);
 * cmd3->redo();
 * cmd3->(obj1, 20);
 * cmd3->redo();
 *
 * auto macro = new MacroCommand{cmd, cmd2, cmd3};
 * commandStack->push(macro);
 * \endcode
 *
 */

/*! \page Serialization
 *
 * \section GenSer Generalities on serialization
 * score has two serialization methods:
 *
 * * A fast one, based on QDataStream
 * * A slow one, based on JSON.
 *
 * If an object of type Foo is serializable, the following functions have to be
reimplemented :
 *
 * In all cases :
 * * `template<> void DataStreamReader::read(const Foo& dom);`
 * * `template<> void DataStreamWriter::write(Foo& dom);`
 *
 * If the object is a "big" object with multiple members, etc:
 * * `template<> void JSONObjectReader::read(const Foo& dom);`
 * * `template<> void JSONObjectWriter::write(Foo& dom);`
 *
 * If the object is a "small" value-like data structure (for instance a (x,y)
array):
 * * `template<> void JSONValueReader::read(const Foo& dom);`
 * * `template<> void JSONValueWriter::write(Foo& dom);`
 *
 * A simple example can be seen in the Engine::Network::MinuitSpecificSettings
serialization code
 * (located in MinuitSpecificSettingsSerialization.cpp).
 *
 * A more complex example would be the Scenario::ProcessModel serialization
code (in ScenarioModelSerialization.cpp).
 *
 * \section DataStreamSer DataStream serialization
 *
 * This is mostly a matter of reading and writing into the `m_stream` variable:
 *
 * \code
 * m_stream << object.member1 << object.member2;
 * \endcode
 *
 * \code
 * m_stream >> object.member1 >> object.member2;
 * \endcode
 *
 * Since this code may be complex, it is possible to introduce
 * a delimiter that will help detecting serialization bugs : just call
 * `insertDelimiter()` in the serialization code and `checkDelimiter()`
 * in the deserialization code.
 *
 * \section JSONObjSer JSON Object serialization
 *
 * Read and write in the `obj` variable, which is a QJsonObject.
 * For instance:
 *
 * \code
 * obj["Bar"] = foo.m_bar;
 * obj["Baz"] = foo.m_baz;
 * \endcode
 *
 * \code
 * foo.m_bar = obj["Bar"].toDouble();
 * foo.m_baz = obj["Bar"].toString();
 * \endcode
 *
 * \section JSONValSer JSON Value serialization
 *
 * Read and write in the `val` variable, which is a QJsonValue.
 * For instance:
 *
 * \code
 * val = QJsonArray{foo.array[0], foo.array[1]};
 * \endcode
 *
 * \code
 * auto arr = val.toArray();
 * foo.array[0] = arr[0].toString();
 * foo.array[1] = arr[1].toString();
 * \endcode
 *
 * \section DeserObj Complex object serialization
 *
 * For data types more complex than ints, floats, & other primitives,
 * for instance inheriting from IdentifiedObject and being part of the object
tree,
 * serialization and deserialization follows the following process:
 *
 * \subsection ObjSer For serializing
 *
 * \subsubsection DSObjSer In DataStream
 *
 * Call `readFrom(theChildObject)`.
 *
 * \subsubsection JSObjSer In JSON
 *
 * Call `toJsonObject(theChildObject)` and save it in a key:
 *
 * \code
   template <>
   void JSONObjectReader::read(const Foo& foo) {
     obj["MyChild"] = toJsonObject(foo.theChildObject());
   }
   \endcode
 *
 * Various utility functions are available in JSONObject.hpp, to save arrays,
etc...
 *
 * \subsection ObjDeser For deserializing
 *
 * \subsubsection DSObjDeser In DataStream
 *
 * Construct the object with the deserializer in argument:
 *
 * \code
   template <>
   void DataStreamWriter::writer(Foo& foo) {
     foo.m_theChildObject = new ChildObject{*this, &foo};
   }
   \endcode
 *
 * The object then deserializes itself in its constructor;
 * see for instance Scenario::IntervalModel::IntervalModel or
 * Scenario::StateModel::StateModel.
 *
 * \subsubsection JSObjDeser In JSON
 *
 * Construct a new deserializer with the child JSON object and pass it
 * to the child constructor:
 *
 * \code
   template <>
   void JSONObjectWriter::writer(Foo& foo) {
     foo.m_theChildObject = new
ChildObject{JSONObject::Deserializer{obj["MyChild"].toObject()}, &foo};
   }
   \endcode
 *
 * \subsection PolySer Serialization of polymorphic types
 *
 * An example is available in Scenario::IntervalModel's serialization code,
which
 * has to serialize its child Process::ProcessModel.
 * The problem here is that we can't just call `new MyObject` since we don't
know the type of the class
 * that we are loading at compile-time.
 *
 * Hence, we have to look for our class in a factory, by saving the UUID of the
class.
 * This is done automatically if the class inherits from
score::SerializableInterface;
 * the serialization code won't change from the "simple" object case.
 *
 * For the deserialization, however, we have to look for the correct factory,
which
 * we can do through the saved UUID, and load the object.
 *
 * This can be done easily through the `deserialize_interface` function:
 *
 * \code
template <>
void DataStreamWriter::write(Scenario::IntervalModel& interval) {
  auto& pl = components.interfaces<Process::ProcessFactoryList>();
  auto proc = deserialize_interface(pl, *this, &interval);
  if(proc) {
   // ...
  } else {
   // handle the error in some way.
  }
}
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
