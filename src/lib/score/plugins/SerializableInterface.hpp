#pragma once
#include <score/plugins/UuidKey.hpp>
#include <score/serialization/VisitorInterface.hpp>

namespace score
{
/**
 * @brief Generic serialization method for abstract classes.
 *
 * The problem is reloading polymorphic classes : we have to save
 * the identifier of the factory used to instantiate them.
 * <br>
 * Base classes should inherit from SerializableInterface, and provide only
 * serialization code for their own data in Visitor<Reader<...>> and
 * Visitor<Writer<...>>. <br> Likewise, subclasses should only save their own
 * data. These classes ensure that everything will be saved in the correct
 * order. <br> See visitor_abstract_tag
 */
template <typename T>
class SerializableInterface
{
public:
  using key_type = UuidKey<T>;
  using is_abstract_base_tag = std::integral_constant<bool, true>;

  SerializableInterface() = default;
  virtual ~SerializableInterface() = default;
  virtual UuidKey<T> concreteKey() const noexcept = 0;

  virtual void serialize_impl(const VisitorVariant& vis) const { }
};
}

/**
 * @macro MODEL_METADATA_IMPL Provides default implementations of methods of
 * SerializableInterface.
 */
#define MODEL_METADATA_IMPL(Model_T)                                              \
  static key_type static_concreteKey() noexcept                                   \
  {                                                                               \
    return Metadata<ConcreteKey_k, Model_T>::get();                               \
  }                                                                               \
  key_type concreteKey() const noexcept override { return static_concreteKey(); } \
  void serialize_impl(const VisitorVariant& vis) const noexcept override          \
  {                                                                               \
    score::serialize_dyn(vis, *this);                                             \
  }

#define MODEL_METADATA_IMPL_HPP(Model_T)          \
  static key_type static_concreteKey() noexcept;  \
  key_type concreteKey() const noexcept override; \
  void serialize_impl(const VisitorVariant& vis) const noexcept override;

#define MODEL_METADATA_IMPL_CPP(Model_T)                                                   \
  Model_T::key_type Model_T::static_concreteKey() noexcept                                 \
  {                                                                                        \
    return Metadata<ConcreteKey_k, Model_T>::get();                                        \
  }                                                                                        \
  Model_T::key_type Model_T::concreteKey() const noexcept { return static_concreteKey(); } \
  void Model_T::serialize_impl(const VisitorVariant& vis) const noexcept                   \
  {                                                                                        \
    score::serialize_dyn(vis, *this);                                                      \
  }
