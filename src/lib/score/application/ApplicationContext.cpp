// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ApplicationContext.hpp"
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

score::ApplicationContext::ApplicationContext(
    const score::ApplicationSettings& app,
    const score::ApplicationComponents& c,
    DocumentList& l,
    const std::vector<std::unique_ptr<score::SettingsDelegateModel>>& set)
    : applicationSettings{app}, components{c}, documents{l}, m_settings{set}
{
}

const score::DocumentContext& score::ApplicationContext::currentDocument() const noexcept
{
  SCORE_ASSERT(documents.currentDocument());
  return documents.currentDocument()->context();
}

score::ApplicationContext::~ApplicationContext() = default;
