#pragma once

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
 * Other plug-ins are available on the github page : https://github.com/ossia/
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
 * https://github.com/ossia/score-addons/blob/master/addons.json
 *
 * Each addon provides a JSON description file with the following keys :
 *
 * * One key per plug-in architecture. The possible names are :
 *   * src
 *   * windows-x86
 *   * windows-amd64
 *   * darwin-amd64
 *   * linux-amd64
 *   * linux-arm
 * * Currently score only supports `src` which means that the plug-in will be built from source.
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
