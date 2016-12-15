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
 * * \ref Factories
 * * \ref Contexts
 * * \ref Commands
 * * \ref Serialization
 * * \ref Documents
 * * \ref Actions
 * * \ref CodingStyle
 * * \ref BuildSystem
 * * \ref PluginSystem
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

/*! \page Factories
 *
 */

/*! \page Contexts
 *
 */

/*! \page Commands
 *
 */

/*! \page Serialization
 *
 */

/*! \page Documents
 *
 */

/*! \page Actions
 *
 */

/*! \page BuildSystem Build system
 *
 */

 /*! \page PluginSystem Plug-in system
 *
 */

/*! \page ModelViewPresenter Model-View-Presenter separation
 *
 */
