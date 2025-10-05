// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "InterfaceList.hpp"

#include <QDebug>

#include <typeinfo>
namespace score
{
InterfaceListBase::~InterfaceListBase() = default;
InterfaceListMain::~InterfaceListMain() = default;
InterfaceListMain::InterfaceListMain() = default;

void InterfaceListMain::insert_base(
    std::unique_ptr<score::InterfaceBase> e, score::uuid_t k)
{
  auto pf = e.get();

  auto it = this->map.find(k);
  if(it == this->map.end())
  {
    this->map.emplace(std::make_pair(k, std::move(e)));
  }
  else
  {
    score::debug_types(it->second.get(), pf);
    it->second = std::move(e);
  }

  added(*pf);
}

void InterfaceListMain::optimize() noexcept
{
  // score::optimize_hash_map(this->map);
  this->map.max_load_factor(0.1f);
  this->map.reserve(map.size());
}

void debug_types(const InterfaceBase* orig, const InterfaceBase* repl) noexcept
{
  qDebug() << "Warning: replacing" << typeid(*orig).name() << "with"
           << typeid(*repl).name();
}

}
