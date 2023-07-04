#pragma once
/**
 * \file QMapHelper.hpp
 *
 * Used to allow nice syntactic features for older version
 * of Qt.
 *
 * TODO instead just stop using QMap and use std::unordered_map.
 */

template <typename T>
struct QMapKeyAdaptor
{
  const T& map;

  auto begin() const { return map.keyBegin(); }

  auto end() const { return map.keyEnd(); }
};

template <typename T>
auto QMap_keys(const T& t)
{
  return QMapKeyAdaptor<T>{t};
}
