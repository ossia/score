#ifndef SCORE_PREFIX_HEADER
#define SCORE_PREFIX_HEADER
#pragma warning(push, 0) // MSVC
#pragma GCC system_header
#pragma clang system_header

/////////////
#include <score/command/AggregateCommand.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreePath.hpp>
#include <score/model/tree/VariantBasedNode.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/string_map.hpp>

#include <QWidget>
#include <QGraphicsItem>

#include <cmath>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include <verdigris>
#include <wobjectimpl.h>
#endif
