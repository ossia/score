#pragma once
#include <iscore_lib_base_export.h>
#include <memory>
class QQuickPaintedItem;
class QQuickPaintedItem;
/**
 * @brief deleteGraphicsItem Properly delete a QQuickPaintedItem
 * @param item item to delete
 *
 * Simply using deleteLater() is generally not enough, the
 * item has to be removed from the scene else there will be crashes.
 */
ISCORE_LIB_BASE_EXPORT void deleteGraphicsObject(QQuickPaintedItem* item);
ISCORE_LIB_BASE_EXPORT void deleteGraphicsItem(QQuickPaintedItem* item);

template <typename T>
struct graphics_item_ptr
{
  T* impl{};
  graphics_item_ptr() = default;
  graphics_item_ptr(const graphics_item_ptr&) = default;
  graphics_item_ptr(graphics_item_ptr&&) = default;
  graphics_item_ptr& operator=(const graphics_item_ptr&) = default;
  graphics_item_ptr& operator=(graphics_item_ptr&&) = default;

  graphics_item_ptr(T* p) : impl{p}
  {
  }

  ~graphics_item_ptr()
  {
    deleteGraphicsItem(impl);
  }

  auto operator=(T* other)
  {
    impl = other;
  }

  operator bool() const
  {
    return impl;
  }

  operator T*() const
  {
    return impl;
  }

  T& operator*() const
  {
    return *impl;
  }

  T* operator->() const
  {
    return impl;
  }
};
