#include "ListeningHandlerFactoryList.hpp"
#include "DefaultListeningHandlerFactory.hpp"
namespace Explorer
{
ListeningHandlerFactoryList::~ListeningHandlerFactoryList()
{

}

ListeningHandler *ListeningHandlerFactoryList::make(
        const iscore::DocumentContext &ctx)
{
    if(m_list.get().empty())
    {
        DefaultListeningHandlerFactory fact;
        return fact.make(ctx);
    }
    else
    {
        for(auto& fact : m_list.get())
        {
            if(auto res = fact.second->make(ctx))
                return res;
        }
        return nullptr;
    }
}
}
