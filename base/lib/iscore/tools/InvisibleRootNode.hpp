#pragma once

struct InvisibleRootNodeTag{};

template<typename T>
class TypeToName;

template<> class TypeToName<InvisibleRootNodeTag>
{ public: static constexpr const char * name() { return "RootNode"; } };
