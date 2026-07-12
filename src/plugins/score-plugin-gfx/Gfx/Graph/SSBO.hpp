#pragma once

#include <isf.hpp>

namespace score::gfx
{
struct Std430TypeInfo
{
  int baseSize{};      // Size of the type itself (e.g., 12 for vec3)
  int baseAlignment{}; // Alignment requirement (e.g., 16 for vec3)

  bool isValid() const { return baseSize > 0 && baseAlignment > 0; }
};

struct LayoutResult
{
  int size{};      // The total size (stride) of the struct/type including padding
  int alignment{}; // The alignment requirement of the struct/type

  bool isValid() const { return size > 0 && alignment > 0; }
};

struct ArrayParseResult
{
  QString baseType;
  int arrayCount{}; // 0 = not an array, -1 = flexible array [], >0 = fixed array [N]
};

static constexpr inline int64_t alignUp(int64_t value, int64_t alignment)
{
  if(alignment <= 0)
    return value;
  return (value + alignment - 1) & ~(alignment - 1);
}

static inline ArrayParseResult parseArrayType(const QString& typeStr)
{
  ArrayParseResult result;
  result.baseType = typeStr;
  result.arrayCount = 0; // Not an array

  int bracketStart = typeStr.lastIndexOf('[');
  int bracketEnd = typeStr.lastIndexOf(']');

  if(bracketStart != -1 && bracketEnd > bracketStart)
  {
    QString content
        = typeStr.mid(bracketStart + 1, bracketEnd - bracketStart - 1).trimmed();
    result.baseType = typeStr.left(bracketStart).trimmed();

    if(content.isEmpty())
    {
      // Flexible array: "type[]"
      result.arrayCount = -1;
    }
    else
    {
      // Fixed array: "type[N]"
      bool ok = false;
      int count = content.toInt(&ok);
      result.arrayCount = ok ? count : 1;
    }
  }

  return result;
}

static inline Std430TypeInfo getStd430BaseTypeInfo(const QString& typeStr)
{
  if(typeStr == "float" || typeStr == "int" || typeStr == "uint" || typeStr == "bool")
    return {4, 4};
  if(typeStr == "double")
    return {8, 8};

  if(typeStr == "vec2" || typeStr == "ivec2" || typeStr == "uvec2" || typeStr == "bvec2")
    return {8, 8};
  if(typeStr == "vec3" || typeStr == "ivec3" || typeStr == "uvec3" || typeStr == "bvec3")
    return {12, 16};
  if(typeStr == "vec4" || typeStr == "ivec4" || typeStr == "uvec4" || typeStr == "bvec4")
    return {16, 16};

  if(typeStr == "dvec2")
    return {16, 16};
  if(typeStr == "dvec3")
    return {24, 32};
  if(typeStr == "dvec4")
    return {32, 32};

  // mat2: 2 columns of vec2, stride=8, total=16
  if(typeStr == "mat2" || typeStr == "mat2x2")
    return {16, 8};
  // mat3: 3 columns of vec3, stride=16 (vec3 aligns to 16), total=48
  if(typeStr == "mat3" || typeStr == "mat3x3")
    return {48, 16};
  // mat4: 4 columns of vec4, stride=16, total=64
  if(typeStr == "mat4" || typeStr == "mat4x4")
    return {64, 16};

  // mat2x3: 2 columns of vec3, stride=16, total=32
  if(typeStr == "mat2x3")
    return {32, 16};
  // mat2x4: 2 columns of vec4, stride=16, total=32
  if(typeStr == "mat2x4")
    return {32, 16};
  // mat3x2: 3 columns of vec2, stride=8, total=24
  if(typeStr == "mat3x2")
    return {24, 8};
  // mat3x4: 3 columns of vec4, stride=16, total=48
  if(typeStr == "mat3x4")
    return {48, 16};
  // mat4x2: 4 columns of vec2, stride=8, total=32
  if(typeStr == "mat4x2")
    return {32, 8};
  // mat4x3: 4 columns of vec3, stride=16, total=64
  if(typeStr == "mat4x3")
    return {64, 16};

  // dmat2: 2 columns of dvec2, stride=16, total=32
  if(typeStr == "dmat2" || typeStr == "dmat2x2")
    return {32, 16};
  // dmat3: 3 columns of dvec3, stride=32 (dvec3 aligns to 32), total=96
  if(typeStr == "dmat3" || typeStr == "dmat3x3")
    return {96, 32};
  // dmat4: 4 columns of dvec4, stride=32, total=128
  if(typeStr == "dmat4" || typeStr == "dmat4x4")
    return {128, 32};

  if(typeStr == "dmat2x3")
    return {64, 32}; // 2 columns of dvec3
  if(typeStr == "dmat2x4")
    return {64, 32}; // 2 columns of dvec4
  if(typeStr == "dmat3x2")
    return {48, 16}; // 3 columns of dvec2
  if(typeStr == "dmat3x4")
    return {96, 32}; // 3 columns of dvec4
  if(typeStr == "dmat4x2")
    return {64, 16}; // 4 columns of dvec2
  if(typeStr == "dmat4x3")
    return {128, 32}; // 4 columns of dvec3

  // Unknown type
  return {0, 0};
}

static inline LayoutResult calculateStructLayout(
    std::span<const isf::storage_input::layout_field> layout,
    std::span<const isf::descriptor::type_definition> typeDefinitions)
{
  if(layout.empty())
    return {0, 0};

  int currentOffset = 0;
  int maxAlignment = 0;

  for(const auto& field : layout)
  {
    const ArrayParseResult parsed = parseArrayType(QString::fromStdString(field.type));
    const QString baseType = parsed.baseType;

    const bool isArray = (parsed.arrayCount != 0);
    const bool isFlexibleArray = (parsed.arrayCount == -1);
    const int arrayCount = (parsed.arrayCount > 0) ? parsed.arrayCount : 1;

    if(isFlexibleArray)
    {
      qWarning() << "Flexible array found inside struct. Invalid GLSL, skipping:"
                 << QString::fromStdString(field.name);
      continue;
    }

    int fieldSize = 0;
    int fieldAlign = 0;

    const Std430TypeInfo info = getStd430BaseTypeInfo(baseType);

    if(info.isValid())
    {
      // Primitive or matrix type
      fieldSize = info.baseSize;
      fieldAlign = info.baseAlignment;
    }
    else
    {
      // Custom struct - search type definitions
      bool found = false;
      for(const auto& typeDef : typeDefinitions)
      {
        if(QString::fromStdString(typeDef.name) == baseType)
        {
          LayoutResult subStruct
              = calculateStructLayout(typeDef.layout, typeDefinitions);
          fieldSize = subStruct.size;
          fieldAlign = subStruct.alignment;
          found = true;
          break;
        }
      }
      if(!found)
      {
        qWarning() << "Unknown type, using fallback alignment:" << baseType;
        fieldSize = 16;
        fieldAlign = 16;
      }
    }

    // --- Handle Array ---
    int totalFieldSize = fieldSize;
    if(isArray && arrayCount > 0)
    {
      // std430: Array element stride = element size rounded up to element alignment
      int elementStride = alignUp(fieldSize, fieldAlign);
      totalFieldSize = elementStride * arrayCount;
    }

    currentOffset = alignUp(currentOffset, fieldAlign);
    currentOffset += totalFieldSize;
    maxAlignment = std::max(maxAlignment, fieldAlign);
  }

  // Struct size must be a multiple of its largest member alignment
  currentOffset = alignUp(currentOffset, maxAlignment);

  return {currentOffset, maxAlignment};
}

// --- std140 (uniform block) layout ---------------------------------------
//
// OpenGL 4.6 core profile, 7.6.2.2 "Standard Uniform Block Layout": std140
// differs from std430 in exactly two rules.
//
//   (4) "If the member is an array of scalars or vectors, the base alignment
//        and array stride are set to match the base alignment of a single
//        array element, according to rules (1), (2), and (3), and rounded up
//        to the base alignment of a vec4."
//   (9) "If the member is a structure, the base alignment of the structure is
//        N, where N is the largest base alignment value of any of its members,
//        and rounded up to the base alignment of a vec4."
//
// and, via rules (5) and (7), a matrix is laid out as an array of column
// vectors, so its columns get the same vec4 rounding.
static constexpr int kStd140Vec4Alignment = 16;

struct MatrixShape
{
  int columns{};
  QString columnType;

  bool isValid() const { return columns > 0; }
};

// "mat3x2" -> 3 columns of vec2; "mat3" -> 3 columns of vec3;
// "dmat4x3" -> 4 columns of dvec3. Anything else -> invalid.
static inline MatrixShape parseMatrixShape(const QString& typeStr)
{
  const bool isDouble = typeStr.startsWith("dmat");
  if(!isDouble && !typeStr.startsWith("mat"))
    return {};

  const QString dims = typeStr.mid(isDouble ? 4 : 3);
  if(dims.isEmpty())
    return {};

  int columns = 0, rows = 0;
  const int x = dims.indexOf('x');
  bool ok = false;
  if(x < 0)
  {
    columns = rows = dims.toInt(&ok);
  }
  else
  {
    columns = dims.left(x).toInt(&ok);
    bool ok2 = false;
    rows = dims.mid(x + 1).toInt(&ok2);
    ok = ok && ok2;
  }
  if(!ok || columns < 2 || columns > 4 || rows < 2 || rows > 4)
    return {};

  return {columns, (isDouble ? QString("dvec") : QString("vec")) + QString::number(rows)};
}

static inline Std430TypeInfo getStd140BaseTypeInfo(const QString& typeStr)
{
  if(const MatrixShape m = parseMatrixShape(typeStr); m.isValid())
  {
    const Std430TypeInfo col = getStd430BaseTypeInfo(m.columnType);
    if(!col.isValid())
      return {};
    const int align = (int)std::max<int64_t>(col.baseAlignment, kStd140Vec4Alignment);
    const int stride = (int)alignUp(col.baseSize, align);
    return {stride * m.columns, align};
  }

  // Scalars and vectors follow the same rules (1)-(3) in std140 and std430.
  return getStd430BaseTypeInfo(typeStr);
}

static inline LayoutResult calculateStructLayout140(
    std::span<const isf::storage_input::layout_field> layout,
    std::span<const isf::descriptor::type_definition> typeDefinitions);

// Size + alignment of one std140 member, before any array multiplication.
static inline LayoutResult std140MemberInfo(
    const QString& baseType,
    std::span<const isf::descriptor::type_definition> typeDefinitions)
{
  if(const Std430TypeInfo info = getStd140BaseTypeInfo(baseType); info.isValid())
    return {info.baseSize, info.baseAlignment};

  for(const auto& typeDef : typeDefinitions)
  {
    if(QString::fromStdString(typeDef.name) == baseType)
      return calculateStructLayout140(typeDef.layout, typeDefinitions);
  }

  qWarning() << "Unknown type in uniform block layout:" << baseType;
  return {kStd140Vec4Alignment, kStd140Vec4Alignment};
}

static inline LayoutResult calculateStructLayout140(
    std::span<const isf::storage_input::layout_field> layout,
    std::span<const isf::descriptor::type_definition> typeDefinitions)
{
  if(layout.empty())
    return {0, 0};

  int64_t currentOffset = 0;
  int64_t maxAlignment = kStd140Vec4Alignment; // rule (9)

  for(const auto& field : layout)
  {
    const ArrayParseResult parsed = parseArrayType(QString::fromStdString(field.type));
    if(parsed.arrayCount == -1)
    {
      qWarning() << "Flexible array found inside std140 struct. Invalid GLSL, skipping:"
                 << QString::fromStdString(field.name);
      continue;
    }

    const LayoutResult member = std140MemberInfo(parsed.baseType, typeDefinitions);
    int64_t fieldAlign = member.alignment;
    int64_t total = member.size;

    if(parsed.arrayCount > 0)
    {
      fieldAlign = std::max<int64_t>(fieldAlign, kStd140Vec4Alignment); // rule (4)
      total = alignUp(member.size, fieldAlign) * parsed.arrayCount;
    }

    currentOffset = alignUp(currentOffset, fieldAlign);
    currentOffset += total;
    maxAlignment = std::max(maxAlignment, fieldAlign);
  }

  return {(int)alignUp(currentOffset, maxAlignment), (int)maxAlignment};
}

/**
 * @brief Byte size of a std140 uniform block declared by @p layout.
 *
 * @p arrayCount is the element count substituted for a trailing flexible
 * array member (`type[]`), matching calculateStorageBufferSize's contract.
 * Returns 0 for an empty layout.
 *
 * Templated over the field range because libisf declares an independent
 * `layout_field` type inside each of storage_input / uniform_input, with
 * identical `name` + `type` members.
 */
template <typename Layout>
static inline int64_t
calculateUniformBlockSize(const Layout& layout, int arrayCount, const isf::descriptor& d)
{
  if(std::empty(layout))
    return 0;

  if(arrayCount < 0)
    arrayCount = 0;

  const auto& typeDefinitions = d.types;

  int64_t currentOffset = 0;
  int64_t maxBufferAlignment = kStd140Vec4Alignment;

  for(const auto& field : layout)
  {
    const ArrayParseResult parsed = parseArrayType(QString::fromStdString(field.type));
    const LayoutResult member = std140MemberInfo(parsed.baseType, typeDefinitions);

    const bool isFlexibleArray = (parsed.arrayCount == -1);
    const bool isFixedArray = (parsed.arrayCount > 0);

    int64_t fieldAlign = member.alignment;
    if(isFlexibleArray || isFixedArray)
      fieldAlign = std::max<int64_t>(fieldAlign, kStd140Vec4Alignment); // rule (4)

    const int64_t elementStride = alignUp(member.size, fieldAlign);

    currentOffset = alignUp(currentOffset, fieldAlign);
    if(isFlexibleArray)
      currentOffset += elementStride * arrayCount;
    else if(isFixedArray)
      currentOffset += elementStride * parsed.arrayCount;
    else
      currentOffset += member.size;

    maxBufferAlignment = std::max(maxBufferAlignment, fieldAlign);
  }

  return alignUp(currentOffset, maxBufferAlignment);
}

static inline int64_t calculateStorageBufferSize(
    std::span<const isf::storage_input::layout_field> layout, int arrayCount,
    const isf::descriptor& d)
{
  if(layout.empty())
    return 0;

  if(arrayCount < 0)
    arrayCount = 0;

  // Get type definitions from the node descriptor
  const auto& typeDefinitions = d.types;

  int64_t currentOffset = 0;
  int64_t maxBufferAlignment = 0;

  for(const auto& field : layout)
  {
    const ArrayParseResult parsed = parseArrayType(QString::fromStdString(field.type));
    const QString baseType = parsed.baseType;

    const bool isFlexibleArray = (parsed.arrayCount == -1);
    const bool isFixedArray = (parsed.arrayCount > 0);
    const int fixedArrayCount = isFixedArray ? parsed.arrayCount : 1;

    int fieldSize = 0;
    int64_t fieldAlign = 0;

    const Std430TypeInfo info = getStd430BaseTypeInfo(baseType);

    if(info.isValid())
    {
      // Primitive or matrix type
      fieldSize = info.baseSize;
      fieldAlign = info.baseAlignment;
    }
    else
    {
      // Custom struct
      bool found = false;
      for(const auto& typeDef : typeDefinitions)
      {
        if(QString::fromStdString(typeDef.name) == baseType)
        {
          const LayoutResult subRes
              = calculateStructLayout(typeDef.layout, typeDefinitions);
          fieldSize = subRes.size;
          fieldAlign = subRes.alignment;
          found = true;
          break;
        }
      }
      if(!found)
      {
        qWarning() << "Unknown type in buffer layout:" << baseType;
        fieldSize = 16;
        fieldAlign = 16;
      }
    }

    int elementStride = alignUp(fieldSize, fieldAlign);
    currentOffset = alignUp(currentOffset, fieldAlign);
    if(isFlexibleArray)
    {
      // Variable-length array: use provided arrayCount
      currentOffset += elementStride * arrayCount;
    }
    else if(isFixedArray)
    {
      // Fixed-length array: use parsed count
      currentOffset += elementStride * fixedArrayCount;
    }
    else
    {
      // Single field (not an array)
      currentOffset += fieldSize;
    }

    maxBufferAlignment = std::max(maxBufferAlignment, fieldAlign);
  }

  currentOffset = alignUp(currentOffset, maxBufferAlignment);

  return currentOffset;
}
}
