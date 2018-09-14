#pragma once
#include "Metadata.hpp"

#include <QString>

#include <score_lib_base_export.h>

/**
 * @brief Generates a random name from the dict.txt file.
 *
 * The current algorithm is :
 *
 * A word + A number + A word + A number
 * and
 * A word + A number for the short version
 *
 * With words being 4 letters and numbers being 2 digits.
 */
class SCORE_LIB_BASE_EXPORT RandomNameProvider
{
public:
  static QString generateRandomName();
  static QString generateShortRandomName();

  template <typename T>
  static QString generateName()
  {
    return Metadata<PrettyName_k, T>::get() + "." + generateShortRandomName();
  }
};
