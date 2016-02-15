#pragma once
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_base_export.h>
/**
 * @brief The InvisibleRootNodeTag struct
 *
 * Used as a type to differentiate the root node in TreeNode-based trees.
 */
struct ISCORE_LIB_BASE_EXPORT InvisibleRootNodeTag{
        friend bool operator==(const InvisibleRootNodeTag&, const InvisibleRootNodeTag&)
        { return true; }
};

JSON_METADATA(InvisibleRootNodeTag, "RootNode")
