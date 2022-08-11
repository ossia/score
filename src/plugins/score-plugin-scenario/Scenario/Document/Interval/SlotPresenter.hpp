#pragma once

#include <Process/HeaderDelegate.hpp>
#include <Process/LayerPresenter.hpp>

#include <Scenario/Document/Interval/LayerData.hpp>
#include <Scenario/Document/Interval/SlotHeader.hpp>

#include <ossia/detail/variant.hpp>

namespace Scenario
{
class SlotHeader;
struct LayerSlotPresenter
{
  SlotHeader* header{};
  Process::HeaderDelegate* headerDelegate{};
  SlotFooter* footer{};
  Process::FooterDelegate* footerDelegate{};
  std::vector<LayerData> layers;

  void cleanupHeaderFooter();
  void cleanup(QGraphicsScene* sc);

  void putToFront(const Id<Process::ProcessModel>&);

  double headerHeight() const noexcept
  {
    if(!header)
      return SlotHeader::headerHeight();

    if(!headerDelegate)
      return header->headerHeight();

    return headerDelegate->boundingRect().height();
  }

  double footerHeight() const noexcept { return SlotFooter::footerHeight(); }
};

class NodalIntervalView;
struct NodalSlotPresenter
{
  SlotHeader* header{};
  SlotFooter* footer{};
  NodalIntervalView* view{};
  double height = 500.;
  void cleanup();
};

struct SlotPresenter : ossia::variant<LayerSlotPresenter, NodalSlotPresenter>
{
  //using variant::variant;
  SlotPresenter() = default;
  SlotPresenter(const SlotPresenter&) = delete;
  SlotPresenter(SlotPresenter&&) noexcept = default;
  SlotPresenter& operator=(const SlotPresenter&) = delete;
  SlotPresenter& operator=(SlotPresenter&&) noexcept = default;

  explicit SlotPresenter(const LayerSlotPresenter& l) noexcept = delete;
  explicit SlotPresenter(const NodalSlotPresenter& l) noexcept = delete;
  explicit SlotPresenter(LayerSlotPresenter&& l) noexcept
      : ossia::variant<LayerSlotPresenter, NodalSlotPresenter>{std::move(l)}
  {
  }
  explicit SlotPresenter(NodalSlotPresenter&& l) noexcept
      : ossia::variant<LayerSlotPresenter, NodalSlotPresenter>{std::move(l)}
  {
  }

  LayerSlotPresenter* getLayerSlot() const noexcept
  {
    return ossia::get_if<LayerSlotPresenter>((variant*)this);
  }
  NodalSlotPresenter* getNodalSlot() const noexcept
  {
    return ossia::get_if<NodalSlotPresenter>((variant*)this);
  }

  template <typename Layer, typename Nodal>
  auto visit(const Layer& lfun, const Nodal& nfun)
  {
    struct
    {
      const Layer& lfun;
      const Nodal& nfun;
      void operator()(LayerSlotPresenter& pres) const { return lfun(pres); }
      void operator()(NodalSlotPresenter& pres) const { return nfun(pres); }
    } visitor{lfun, nfun};
    return ossia::visit(visitor, (variant&)*this);
  }

  template <typename Layer, typename Nodal>
  auto visit(const Layer& lfun, const Nodal& nfun) const
  {
    struct
    {
      const Layer& lfun;
      const Nodal& nfun;
      void operator()(const LayerSlotPresenter& pres) const { return lfun(pres); }
      void operator()(const NodalSlotPresenter& pres) const { return nfun(pres); }
    } visitor{lfun, nfun};
    return ossia::visit(visitor, (const variant&)*this);
  }

  template <typename F>
  auto visit(const F& fun)
  {
    return ossia::visit(fun, (variant&)*this);
  }

  template <typename F>
  auto visit(const F& fun) const
  {
    return ossia::visit(fun, (const variant&)*this);
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
