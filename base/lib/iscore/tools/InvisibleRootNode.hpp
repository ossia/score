#pragma once
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_base_export.h>
/**
 * @brief The InvisibleRootNodeTag struct
 *
 * Used as a type to differentiate the root node in TreeNode-based trees.
 */
struct ISCORE_LIB_BASE_EXPORT InvisibleRootNode
{
  friend bool
  operator==(const InvisibleRootNode&, const InvisibleRootNode&)
  {
    return true;
  }
};

JSON_METADATA(InvisibleRootNode, "RootNode")
