#pragma once

/**
 * @brief The InvisibleRootNodeTag struct
 *
 * Used as a type to differentiate the root node in TreeNode-based trees.
 */
struct InvisibleRootNodeTag{
        friend bool operator==(const InvisibleRootNodeTag&, const InvisibleRootNodeTag&)
        { return true; }
};

template<typename T>
class TypeToName;

template<> class TypeToName<InvisibleRootNodeTag>
{ public: static constexpr const char * name() { return "RootNode"; } };
