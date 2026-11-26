/* Cr1stal Client: shared colors and shapes of the menu design. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_STYLE_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_STYLE_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <functional>
#include <string>

namespace CCStyle
{

	// big dark panel behind a whole page
	inline constexpr ColorRGBA PANEL = ColorRGBA(0.06f, 0.055f, 0.065f, 0.88f);
	// rounded rectangle for one group of settings
	inline constexpr ColorRGBA CARD = ColorRGBA(0.085f, 0.08f, 0.09f, 0.95f);
	inline constexpr ColorRGBA CARD_EDGE = ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f);
	// the theme color (crimson by default) for the selected thing, see ApplyTheme
	inline ColorRGBA ACCENT = ColorRGBA(0.86f, 0.07f, 0.23f, 0.92f);
	inline ColorRGBA ACCENT_SOFT = ColorRGBA(0.86f, 0.07f, 0.23f, 0.22f);
	// lighter theme color for titles
	inline ColorRGBA ACCENT_TEXT = ColorRGBA(0.95f, 0.18f, 0.33f, 1.0f);
	// darker theme color for the neon background
	inline ColorRGBA ACCENT_DEEP = ColorRGBA(0.80f, 0.04f, 0.15f, 1.0f);

	// 0 crimson, 1 ocean, 2 violet, 3 emerald, 4 gold, 5 ice
	enum
	{
		NUM_THEMES = 6
	};
	void ApplyTheme(int Theme);
	inline constexpr ColorRGBA HOVER = ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f);
	inline constexpr ColorRGBA BUTTON = ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f);
	inline constexpr ColorRGBA TEXT = ColorRGBA(0.95f, 0.94f, 0.96f, 1.0f);
	inline constexpr ColorRGBA TEXT_DIM = ColorRGBA(0.62f, 0.6f, 0.65f, 1.0f);
	inline constexpr ColorRGBA TOGGLE_ON = ColorRGBA(0.204f, 0.78f, 0.349f, 1.0f);
	inline constexpr ColorRGBA TOGGLE_OFF = ColorRGBA(0.24f, 0.23f, 0.26f, 1.0f);

	inline constexpr float CARD_ROUNDING = 12.0f;

	inline ColorRGBA Mix(ColorRGBA A, ColorRGBA B, float T)
	{
		return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
	}

	// card without any frame; Glow 0..1 tints it softly red (for hover)
	inline void DrawCard(const CUIRect &Rect, float Rounding = CARD_ROUNDING, ColorRGBA Color = CARD, float Glow = 0.0f)
	{
		Rect.Draw(Mix(Color, ColorRGBA(0.2f, 0.05f, 0.09f, Color.a), std::clamp(Glow, 0.0f, 1.0f) * 0.7f), IGraphics::CORNER_ALL, Rounding);
	}

	// thin neon light around text or a shape: Draw is called with small offsets in a circle and a
	// faint color, so the copies blend into a soft edge of about one to two millimeters.
	// Strength 0..1 (use NeonPulse for a slow breathing).
	void NeonGlow(const std::function<void(vec2 Offset, ColorRGBA Color)> &Draw, ColorRGBA Glow, float Strength, float Radius = 1.6f);
	// slow, smooth breathing between 0.65 and 1 (Phase shifts it per element)
	float NeonPulse(float Time, float Phase = 0.0f);

	// the current texture on a rectangle with round corners (call between nothing, it does its own QuadsBegin/End)
	void TexturedRoundRect(IGraphics *pGraphics, float X, float Y, float W, float H, float Rounding, ColorRGBA Color = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

	// upper case for latin and cyrillic text, for small titles like PLAYTIME
	std::string Upper(const char *pText);

} // namespace CCStyle

#endif
