#pragma once
#include "mesh.hpp"
#include "node.hpp"

#include <QtShaderTools/QShaderBaker>
struct FilterNode : NodeModel
{
  const TexturedTriangle& m_mesh = TexturedTriangle::instance();
  FilterNode(QString frag)
  {
    setShaders(m_mesh.defaultVertexShader(), frag);

    const auto& d = m_fragmentS.description();
    for (auto& ub : d.combinedImageSamplers())
    {
      input.push_back(new Port{this, {}, Types::Image, {}});
    }
    for (auto& ub : d.uniformBlocks())
    {
      if (ub.blockName != "material_t")
        continue;

      int sz = 0;
      for (auto& u : ub.members)
      {
        switch (u.type)
        {
          case QShaderDescription::Int:
          case QShaderDescription::Float:
            sz += 4;
            break;
          case QShaderDescription::Int2:
          case QShaderDescription::Vec2:
            sz += 8;
            if (sz % 8 != 0)
              sz += 4;
            break;
          case QShaderDescription::Int3:
          case QShaderDescription::Vec3:
            sz += 12;
            break;
          case QShaderDescription::Int4:
          case QShaderDescription::Vec4:
            sz += 16;
            break;

          default:
            qDebug() << "Warning ! " << u.name << "not handled ! things will go wrong !";
            break;
        }
      }

      m_materialData.reset(new char[sz]);
      std::fill_n(m_materialData.get(), sz, 0);

      char* cur = m_materialData.get();
      char* orig = cur;
      for (auto& u : ub.members)
      {
        switch (u.type)
        {
          case QShaderDescription::Int:
            input.push_back(new Port{this, cur, Types::Int, {}});
            cur += 4;
            break;
          case QShaderDescription::Float:
            input.push_back(new Port{this, cur, Types::Float, {}});
            cur += 4;
            break;
          case QShaderDescription::Int2:
          case QShaderDescription::Vec2:
            input.push_back(new Port{this, cur, Types::Vec2, {}});
            cur += 8;
            if ((cur - orig) % 8 != 0)
              cur += 4;
            break;
          case QShaderDescription::Int3:
          case QShaderDescription::Vec3:
            input.push_back(new Port{this, cur, Types::Vec3, {}});
            cur += 12;
            break;
          case QShaderDescription::Int4:
          case QShaderDescription::Vec4:
            input.push_back(new Port{this, cur, Types::Vec4, {}});
            cur += 16;
            break;

          default:
            qDebug() << "Warning ! " << u.name << "not handled ! things will go wrong !";
            break;
        }
      }
    }

    output.push_back(new Port{this, {}, Types::Image, {}});
  }

  const Mesh& mesh() const noexcept override { return this->m_mesh; }
  virtual ~FilterNode();
};
