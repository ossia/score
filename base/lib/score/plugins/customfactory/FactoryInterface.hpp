#pragma once
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score_lib_base_export.h>

namespace score
{

class InterfaceBase;
using InterfaceKey = UuidKey<InterfaceBase>;
/**
 * @brief Base class for plug-in interfaces
 *
 * The plug-in interfaces in score are organized as follows :
 *
 * * Each base interface should inherit from Interface<MyInterface>
 * * There is a key type to identify each interface type : score::InterfaceKey
 * * Then, each implementation of each interface should itself provide a key to identify
 *   itself with regards to the other implementations.
 *
 * This then allows to :
 *
 * * Fetch a specific interface from an score::ApplicationContext :
 *
 * @code
 * // First get the interface list we are looking for, for instance
 * // the list of all process factories interface :
 * score::ApplicationContext& context = ...;
 * auto& iface_list = context.interfaces<MyInterfaceList>();
 * @endcode
 *
 * * Then look for the actual factory we want, obtained from user input for instance :
 *
 * @code
 * auto iface = iface_list.get(actual_factory_key);
 *
 * // If iface is != nullptr, it means that we have a concrete factory to build
 * // a specific kind of process.
 *
 * @endcode
 *
 * The actual hierarchy looks like :
 *
 * @code
 * InterfaceBase <- Interface<T> <- MyInterface <- MyImplementation
 *                               <- OtherInterface <- OtherImplementation1
 *                                                 <- OtherImplementation2
 * @endcode
 *
 */
class SCORE_LIB_BASE_EXPORT InterfaceBase
{
public:
  virtual ~InterfaceBase();

  //! Identifies an interface uniquely.
  virtual InterfaceKey interfaceKey() const noexcept = 0;
};

template <typename T>
/**
 * @brief Base class to use for interfaces.
 *
 * This class adds a mean to identify its concrete implementations
 * in order to choose them from a list.
 *
 * One should inherit from this when creating a new plug-in interface.
 */
class Interface : public InterfaceBase
{
public:
  virtual ~Interface() = default;

  using ConcreteKey = UuidKey<T>;

  //! Identifies an implementation of an interface uniquely
  virtual ConcreteKey concreteKey() const noexcept = 0;
};
}

/**
 * \macro SCORE_INTERFACE
 * \brief Default implementation code for InterfaceBase.
 *
 * Use this macro in the class definition of classes inheriting from
 * Interface<T>.
 *
 * An uuid should be passed in argument.
 *
 */
#define SCORE_INTERFACE(Uuid)                                  \
public:                                                                \
  static Q_DECL_RELAXED_CONSTEXPR score::InterfaceKey           \
  static_interfaceKey() noexcept                                           \
  {                                                                    \
    return_uuid(Uuid);                                                 \
  }                                                                    \
                                                                       \
  score::InterfaceKey interfaceKey() const noexcept final override \
  {                                                                    \
    return static_interfaceKey();                                \
  }                                                                    \
private: \

/**
 * \macro SCORE_CONCRETE
 * \brief Default implementation for Interface<T>.
 *
 * Use this macro in the class definition inheriting from
 * your interfaces.
 */
#define SCORE_CONCRETE(Uuid)                          \
public:                                                        \
  static Q_DECL_RELAXED_CONSTEXPR ConcreteKey           \
  static_concreteKey() noexcept                                   \
  {                                                            \
    return_uuid(Uuid);                                         \
  }                                                            \
                                                               \
  ConcreteKey concreteKey() const noexcept final override \
  {                                                            \
    return static_concreteKey();                        \
  }                                                            \
private: \


