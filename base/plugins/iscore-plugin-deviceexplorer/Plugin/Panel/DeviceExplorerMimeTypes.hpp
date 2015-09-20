#pragma once


namespace iscore
{
    namespace mime
    {
        // TODO give a definition of what's expected to be in each mime type somewhere.
        inline constexpr const char * nodes()  { return "application/x-iscore-nodes"; }
        inline constexpr const char * device()  { return "application/x-iscore-device"; }
        inline constexpr const char * xml_namespace() { return "application/x-iscore-xml-namespace"; }
        inline constexpr const char * address() { return "application/x-iscore-address"; }
    }
}
