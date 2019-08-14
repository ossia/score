#pragma once
#include <QObject>

template <typename T>
struct matches
{
  bool operator()(const QObject* obj) { return dynamic_cast<const T*>(obj); }
};

/**
 * @brief matches
 * @return <= 0 : does not match
 * > 0 : matches. The highest priority should be taken.
 */
using Priority = int32_t;
