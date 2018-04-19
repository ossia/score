#pragma once
#include <score/tools/Metadata.hpp>
#include <score_lib_base_export.h>
/**
 * @brief The InvisibleRootNodeTag struct
 *
 * Used as a type to differentiate the root node in TreeNode-based trees.
 */
struct SCORE_LIB_BASE_EXPORT InvisibleRootNode
{
  friend bool operator==(const InvisibleRootNode&, const InvisibleRootNode&)
  {
    return true;
  }
};

JSON_METADATA(InvisibleRootNode, "RootNode")
