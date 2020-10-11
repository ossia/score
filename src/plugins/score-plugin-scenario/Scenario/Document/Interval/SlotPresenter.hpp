#pragma once

#include <Process/HeaderDelegate.hpp>
#include <Process/LayerPresenter.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>
#include <variant>

namespace Scenario
{
class LayerData;
class SlotHeader;
struct LayerSlotPresenter {
  SlotHeader* header{};
  Process::HeaderDelegate* headerDelegate{};
  SlotFooter* footer{};
  Process::FooterDelegate* footerDelegate{};
  std::vector<LayerData> layers;

  void cleanupHeaderFooter();
  void cleanup(QGraphicsScene* sc);

  double headerHeight() const noexcept
  {
    if (!header)
      return SlotHeader::headerHeight();

    if (!headerDelegate)
      return header->headerHeight();

    return headerDelegate->boundingRect().height();
  }

  double footerHeight() const noexcept { return SlotFooter::footerHeight(); }
};

class NodalIntervalView;
struct NodalSlotPresenter {
  SlotHeader* header{};
  SlotFooter* footer{};
  NodalIntervalView* view{};
  double height = 500.;
  void cleanup();
};

struct SlotPresenter: std::variant<LayerSlotPresenter, NodalSlotPresenter>
{
  using variant::variant;

  LayerSlotPresenter* getLayerSlot() const noexcept
  {
    return std::get_if<LayerSlotPresenter>((variant*)this);
  }
  NodalSlotPresenter* getNodalSlot() const noexcept
  {
    return std::get_if<NodalSlotPresenter>((variant*)this);
  }

  template<typename Layer, typename Nodal>
  auto visit(const Layer& lfun, const Nodal& nfun)
  {
    struct {
      const Layer& lfun;
      const Nodal& nfun;
      void operator()(LayerSlotPresenter& pres) const {
        return lfun(pres);
      }
      void operator()(NodalSlotPresenter& pres) const {
        return nfun(pres);
      }
    } visitor{lfun, nfun};
    return std::visit(visitor, (variant&)*this);
  }

  template<typename Layer, typename Nodal>
  auto visit(const Layer& lfun, const Nodal& nfun) const
  {
    struct {
      const Layer& lfun;
      const Nodal& nfun;
      void operator()(const LayerSlotPresenter& pres) const {
        return lfun(pres);
      }
      void operator()(const NodalSlotPresenter& pres) const {
        return nfun(pres);
      }
    } visitor{lfun, nfun};
    return std::visit(visitor, (const variant&)*this);
  }

  template<typename F>
  auto visit(const F& fun)
  {
    return std::visit(fun, (variant&)*this);
  }

  template<typename F>
  auto visit(const F& fun) const
  {
    return std::visit(fun, (const variant&)*this);
  }

  void cleanup(QGraphicsScene* sc)
  {
    if(auto p = getLayerSlot())
      p->cleanup(sc);
    else if(auto p = getNodalSlot())
      p->cleanup();
  }
};

}
