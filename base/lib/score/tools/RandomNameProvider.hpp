#pragma once
#include <QString>
#include <score_lib_base_export.h>

/**
 * @brief Generates a random name from the dict.txt file.
 *
 * The current algorithm is :
 *
 * A word + A number + A word + A number
 *
 * With words being 4 letters and numbers being 2 digits.
 */
class SCORE_LIB_BASE_EXPORT RandomNameProvider
{
public:
  static QString generateRandomName();
};
