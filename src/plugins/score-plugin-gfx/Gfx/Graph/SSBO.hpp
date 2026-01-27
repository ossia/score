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

static constexpr inline int alignUp(int value, int alignment)
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

int calculateStorageBufferSize(
    std::span<const isf::storage_input::layout_field> layout, int arrayCount,
    const isf::descriptor& d)
{
  if(layout.empty())
    return 0;

  if(arrayCount < 0)
    arrayCount = 0;

  // Get type definitions from the node descriptor
  const auto& typeDefinitions = d.types;

  int currentOffset = 0;
  int maxBufferAlignment = 0;

  for(const auto& field : layout)
  {
    const ArrayParseResult parsed = parseArrayType(QString::fromStdString(field.type));
    const QString baseType = parsed.baseType;

    const bool isFlexibleArray = (parsed.arrayCount == -1);
    const bool isFixedArray = (parsed.arrayCount > 0);
    const int fixedArrayCount = isFixedArray ? parsed.arrayCount : 1;

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
