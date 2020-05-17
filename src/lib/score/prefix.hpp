#ifndef SCORE_PREFIX_HEADER
#define SCORE_PREFIX_HEADER
#pragma warning(push, 0) // MSVC
#pragma GCC system_header
#pragma clang system_header

#if defined(_MSC_VER)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif
/////////////////////

#include <score/command/AggregateCommand.hpp>
#include <score/model/EntityImpl.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreePath.hpp>
#include <score/model/tree/VariantBasedNode.hpp>
#include <score/selection/Selection.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/serialization/IsTemplate.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/string_map.hpp>
#include <ossia/detail/algorithms.hpp>

#include <QGraphicsItem>
#include <QWidget>

#include <cmath>
#include <wobjectimpl.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <string>
#include <utility>
#include <vector>
#include <verdigris>
#endif
