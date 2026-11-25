/* Cr1stal Client: shared colors and shapes of the new menu design. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_STYLE_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_STYLE_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

namespace CCStyle
{

	// big dark panel behind a whole page
	inline constexpr ColorRGBA PANEL = ColorRGBA(0.045f, 0.035f, 0.085f, 0.82f);
	// rounded rectangle for one group of settings
	inline constexpr ColorRGBA CARD = ColorRGBA(0.105f, 0.09f, 0.165f, 0.92f);
	inline constexpr ColorRGBA CARD_EDGE = ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f);
	// calm blue violet for the selected thing, no pink
	inline constexpr ColorRGBA ACCENT = ColorRGBA(0.42f, 0.37f, 1.0f, 0.9f);
	inline constexpr ColorRGBA ACCENT_SOFT = ColorRGBA(0.42f, 0.37f, 1.0f, 0.3f);
	inline constexpr ColorRGBA HOVER = ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f);
	inline constexpr ColorRGBA BUTTON = ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f);
	inline constexpr ColorRGBA TEXT = ColorRGBA(0.95f, 0.93f, 0.98f, 1.0f);
	inline constexpr ColorRGBA TEXT_DIM = ColorRGBA(0.68f, 0.66f, 0.78f, 1.0f);
	inline constexpr ColorRGBA TOGGLE_ON = ColorRGBA(0.204f, 0.78f, 0.349f, 1.0f);
	inline constexpr ColorRGBA TOGGLE_OFF = ColorRGBA(0.23f, 0.21f, 0.28f, 1.0f);

	inline constexpr float CARD_ROUNDING = 12.0f;

	// card with a thin light edge
	inline void DrawCard(const CUIRect &Rect, float Rounding = CARD_ROUNDING, ColorRGBA Color = CARD)
	{
		CUIRect Edge = Rect;
		Edge.Draw(CARD_EDGE, IGraphics::CORNER_ALL, Rounding);
		CUIRect Inner;
		Rect.Margin(0.75f, &Inner);
		Inner.Draw(Color, IGraphics::CORNER_ALL, Rounding - 0.75f);
	}

} // namespace CCStyle

#endif
