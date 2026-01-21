#pragma once
#include <score/config.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/selection/Selection.hpp>

#include <QPainter>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT BackgroundRenderer : public QObject
{
public:
  using QObject::QObject;
  ~BackgroundRenderer() override;

  virtual bool render(QPainter* painter, const QRectF& rect) = 0;
};

class SCORE_LIB_BASE_EXPORT BackgroundRendererFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(BackgroundRendererFactory, "c9f82f26-7dec-4b87-a66f-3b883d27682a")
public:
  ~BackgroundRendererFactory() override;

  virtual bool matches(const Selection& cst, QObject* parent) const noexcept = 0;
  virtual BackgroundRenderer* make(const Selection& sel, QObject* parent) const = 0;
};

class SCORE_LIB_BASE_EXPORT BackgroundRendererList final
    : public score::MatchingFactory<BackgroundRendererFactory>
{
public:
  ~BackgroundRendererList() override;
};
}
