#pragma once
#include <QStringList>
#include <iscore/tools/Version.hpp>
#include <iscore_lib_base_export.h>
namespace iscore
{
/**
 * @brief Load-time settings
 *
 * Core application settings that are set on the command line.
 * The actual options names are given in the ApplicationSettings::parse
 * implementation.
 */
struct ISCORE_LIB_BASE_EXPORT ApplicationSettings
{
  //! If true, will ask the user if he wants to restore upon loading.
  bool tryToRestore = true;

  //! If true, will show the GUI upon loading.
  bool gui = true;

  //! If true, will start playing after loading the scenarios
  bool autoplay = false;

  //! The version of the base i-score framework's JSON save file.
  iscore::Version saveFormatVersion{2};

  //! List of scenarios that should be loaded
  QStringList loadList;

  void parse();
};
}
