// river_qa --shade (shade.cpp): the river flow shading of a rectangle as images.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <anno/render/scene.hpp>

/// Writes <out>/<name>_g<group>_t<time>_light.png for each time and
/// <name>_g<group>_quiver.png for the scene rectangle `view` at `group`, and
/// prints one Markdown row (river pixels, pixels with a flow, coverage). `gain`
/// multiplies the ripple light (RiverConfig::light_gain).
bool run_shade(const anno::render::Scene& scene, std::size_t group, anno::render::Rect view,
               const std::vector<std::uint64_t>& times, const std::filesystem::path& out,
               const std::string& name, float gain = 1.0F);
