#pragma once
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
