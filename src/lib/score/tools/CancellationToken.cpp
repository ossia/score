#include <score/tools/CancellationToken.hpp>

#include <algorithm>
#include <vector>

namespace score
{
namespace
{
struct LiveTokens
{
  std::mutex mutex;
  std::vector<std::weak_ptr<CancellationToken>> tokens;
};
LiveTokens& liveTokens()
{
  static LiveTokens t;
  return t;
}
}

std::shared_ptr<CancellationToken> CancellationToken::create()
{
  auto token = std::make_shared<CancellationToken>();
  auto& live = liveTokens();
  std::lock_guard lock{live.mutex};
  std::erase_if(live.tokens, [](const auto& t) { return t.expired(); });
  live.tokens.push_back(token);
  return token;
}

void CancellationToken::cancel() noexcept
{
  m_cancelled = true;
  std::lock_guard lock{m_mutex};
}

void CancellationToken::cancelAll() noexcept
{
  std::vector<std::shared_ptr<CancellationToken>> tokens;
  {
    auto& live = liveTokens();
    std::lock_guard lock{live.mutex};
    for(auto& t : live.tokens)
      if(auto p = t.lock())
        tokens.push_back(std::move(p));
    live.tokens.clear();
  }
  for(auto& t : tokens)
    t->cancel();
}
}
