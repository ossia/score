#include "AreaFactoryList.hpp"
#include "AreaFactory.hpp"
#include <boost/range/algorithm/find_if.hpp>
AreaFactory*AreaFactoryList::factory(int type) const
{
    auto it = boost::range::find_if(
                  m_factories,
                  [&type](AreaFactory * p)
    {
        return p->type() == type;
    });

    return it != m_factories.end() ? *it : nullptr;
}

AreaFactory* AreaFactoryList::factory(const QString& name) const
{
    auto it = boost::range::find_if(
                  m_factories,
                  [&name](AreaFactory * p)
    {
        return p->name() == name;
    });

    return it != m_factories.end() ? *it : nullptr;
}

void AreaFactoryList::registerFactory(iscore::FactoryInterface* arg)
{
    auto p = static_cast<AreaFactory*>(arg);
    auto it = boost::range::find_if(
                  m_factories,
                  [&p](AreaFactory * inner_p)
    {
        return inner_p->name() == p->name();
    });

    if(it == m_factories.end())
    {
        m_factories.push_back(p);
    }
    else
    {
        qWarning() << "Alert : an area with the name" << p->name() << "already exists.";
    }

}

std::vector<AreaFactory*> AreaFactoryList::factories() const
{ return m_factories; }
