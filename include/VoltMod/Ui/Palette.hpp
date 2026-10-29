#pragma once

#include <string_view>

/**
 * @brief The default theme's colours for center HTML and other text markup.
 *
 * Mirrors the tokens of the same names in templates/panorama/blocks/theme.css.j2, so a fallback
 * menu or panel matches the Panorama screens; a CLI test keeps the two equal.
 */
namespace VoltMod::Palette
{

inline constexpr std::string_view Accent = "#e1273c";
inline constexpr std::string_view AccentBright = "#ff4a5c";
inline constexpr std::string_view Highlight = "#e0a040";
inline constexpr std::string_view Success = "#4cc26a";
inline constexpr std::string_view TextDefault = "#f7f4ee";
inline constexpr std::string_view TextMuted = "#9a9aa4";
inline constexpr std::string_view TextDisabled = "#5e5e68";

}  // namespace VoltMod::Palette
