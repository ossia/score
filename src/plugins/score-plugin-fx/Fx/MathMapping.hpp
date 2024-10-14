#pragma once

#if FX_UI
namespace Control
{
template <>
struct HasCustomUI<Nodes::MathAudioFilter::Node> : std::true_type
{
};
template <>
struct HasCustomUI<Nodes::MathAudioGenerator::Node> : std::true_type
{
};
template <>
struct HasCustomUI<Nodes::MathMapping::Node> : std::true_type
{
};
template <>
struct HasCustomUI<Nodes::MicroMapping::Node> : std::true_type
{
};
template <>
struct HasCustomUI<Nodes::ArrayGenerator::Node> : std::true_type
{
};
template <>
struct HasCustomUI<Nodes::ArrayMapping::Node> : std::true_type
{
};
template <>
struct HasCustomUI<Nodes::MathGenerator::Node> : std::true_type
{
};
}
#endif
