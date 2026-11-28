/* Cr1stal Client: dark menu design with neon glow. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_NEON_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_NEON_H

#include <base/color.h>

#include <engine/graphics.h>

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>

// Every shape here is made only of rounded rectangles: the glow is a few faint rectangles
// stacked around a shape, so nothing needs a blur or a texture and it stays sharp in 4K.
namespace CCNeon
{

// true while the menu is drawn with the new design (the menu scope sets the rounding scale)
inline bool On()
{
	return CUIRect::RoundingScale() > 1.0f;
}

inline constexpr ColorRGBA Rgb(unsigned Hex, float Alpha = 1.0f)
{
	return ColorRGBA(((Hex >> 16) & 0xFF) / 255.0f, ((Hex >> 8) & 0xFF) / 255.0f, (Hex & 0xFF) / 255.0f, Alpha);
}

inline constexpr ColorRGBA BG_TOP = Rgb(0x120F18);
inline constexpr ColorRGBA BG_BOTTOM = Rgb(0x08060B);
inline constexpr ColorRGBA PANEL = Rgb(0x16121D, 0.95f);
inline constexpr ColorRGBA CARD = Rgb(0x1D1826, 0.96f);
inline constexpr ColorRGBA ELEVATED = Rgb(0x271F31);
inline constexpr ColorRGBA HOVER = Rgb(0x32283E);
inline constexpr ColorRGBA PRESSED = Rgb(0x181320);
inline constexpr ColorRGBA INPUT = Rgb(0x120E18);
inline constexpr ColorRGBA LINE = ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f);
inline constexpr ColorRGBA TEXT = Rgb(0xF7F2F8);
inline constexpr ColorRGBA TEXT_SUB = Rgb(0xB5A9C0);
inline constexpr ColorRGBA TEXT_DIM = Rgb(0x948AA0);
inline constexpr ColorRGBA ACCENT = Rgb(0xE8194C);
inline constexpr ColorRGBA ACCENT_HI = Rgb(0xFF4D78);
inline constexpr ColorRGBA ACCENT_TEXT = Rgb(0xFF7C9E);
inline constexpr ColorRGBA ACCENT_LO = Rgb(0xB3123A);
inline constexpr ColorRGBA NEON = Rgb(0xFF285A);
inline constexpr ColorRGBA OFF = Rgb(0x3C3346);
inline constexpr ColorRGBA TRACK = Rgb(0x33293E);
inline constexpr ColorRGBA PRIMARY_EDGE = Rgb(0xFF7C9E, 0.75f);

enum
{
	KIND_BUTTON = 0,
	KIND_CHECKBOX,
	KIND_SCROLLBAR,
	KIND_DROPDOWN,
	KIND_EDIT,
	KIND_BIND,
};

inline ColorRGBA Lerp(ColorRGBA A, ColorRGBA B, float T)
{
	T = std::clamp(T, 0.0f, 1.0f);
	return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
}

inline bool IsGrey(ColorRGBA Color)
{
	return std::fabs(Color.r - Color.g) <= 0.02f && std::fabs(Color.g - Color.b) <= 0.02f;
}

// Draws with exactly the given rounding (the menu normally makes all rounding rounder).
struct CExactRounding
{
	float m_Prev;
	CExactRounding()
	{
		m_Prev = CUIRect::RoundingScale();
		CUIRect::SetRoundingScale(1.0f);
	}
	~CExactRounding() { CUIRect::SetRoundingScale(m_Prev); }
	CExactRounding(const CExactRounding &) = delete;
	CExactRounding &operator=(const CExactRounding &) = delete;
};

// Neon glow around a rounded rectangle: Layers faint copies, each one Step units bigger.
// Rounding is the final rounding of the shape itself. Call before drawing the shape.
inline void Glow(const CUIRect &Rect, float Rounding, ColorRGBA Color, float Strength = 1.0f, int Corners = IGraphics::CORNER_ALL, float Step = 1.25f, int Layers = 4)
{
	if(Strength <= 0.0f)
		return;
	const CExactRounding Exact;
	for(int i = Layers; i >= 1; i--)
	{
		const float Grow = i * Step;
		CUIRect Outer;
		Outer.x = Rect.x - Grow;
		Outer.y = Rect.y - Grow;
		Outer.w = Rect.w + Grow * 2.0f;
		Outer.h = Rect.h + Grow * 2.0f;
		ColorRGBA Layer = Color;
		Layer.a = Color.a * Strength * 0.06f;
		Outer.Draw(Layer, Corners, std::min(Rounding + Grow, std::min(Outer.w, Outer.h) / 2.0f));
	}
}

// Filled rounded rectangle with a thin light edge.
inline void DrawFramed(const CUIRect &Rect, ColorRGBA Fill, ColorRGBA Edge, float Rounding, int Corners = IGraphics::CORNER_ALL, float EdgeWidth = 0.5f)
{
	const CExactRounding Exact;
	const float R = std::min(Rounding, std::min(Rect.w, Rect.h) / 2.0f);
	if(Edge.a > 0.0f)
	{
		CUIRect Outer;
		Outer.x = Rect.x - EdgeWidth;
		Outer.y = Rect.y - EdgeWidth;
		Outer.w = Rect.w + EdgeWidth * 2.0f;
		Outer.h = Rect.h + EdgeWidth * 2.0f;
		Outer.Draw(Edge, Corners, R + EdgeWidth);
	}
	Rect.Draw(Fill, Corners, R);
}

// Rounding of the new design: the same scale as everything else in the menu.
inline float Scaled(float Rounding, const CUIRect &Rect)
{
	return std::min(Rounding * 1.8f, std::min(Rect.w, Rect.h) / 2.0f);
}

// Flat accent button (for the one main action of a page) with glow.
inline void DrawPrimary(const CUIRect &Rect, bool Hot, bool Active, int Corners = IGraphics::CORNER_ALL, float Rounding = 5.0f)
{
	const float R = Scaled(Rounding, Rect);
	Glow(Rect, R, NEON, Hot ? 1.5f : 1.0f, Corners);
	DrawFramed(Rect, Active ? ACCENT_LO : (Hot ? ACCENT_HI : ACCENT), PRIMARY_EDGE, R, Corners);
}

// Ordinary dark button.
inline void DrawSecondary(const CUIRect &Rect, bool Hot, bool Active, int Corners = IGraphics::CORNER_ALL, float Rounding = 5.0f, float Fade = 1.0f)
{
	ColorRGBA Fill = Active ? PRESSED : (Hot ? HOVER : ELEVATED);
	Fill.a *= Fade;
	ColorRGBA Edge = Hot ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.14f) : LINE;
	Edge.a *= Fade;
	DrawFramed(Rect, Fill, Edge, Scaled(Rounding, Rect), Corners);
}

// Soft selected look: accent fill with glow (selected tab, selected list row).
inline void DrawSelected(const CUIRect &Rect, float Rounding, int Corners = IGraphics::CORNER_ALL, float Strength = 1.0f)
{
	const float R = Scaled(Rounding, Rect);
	Glow(Rect, R, NEON, 0.55f * Strength, Corners, 1.0f, 3);
	DrawFramed(Rect, ColorRGBA(ACCENT.r, ACCENT.g, ACCENT.b, 0.30f * Strength), ColorRGBA(NEON.r, NEON.g, NEON.b, 0.35f * Strength), R, Corners);
}

// Round toggle: Anim 0 = off, 1 = on.
inline void DrawToggle(const CUIRect &Rect, float Anim, bool Hot)
{
	const float R = Rect.h / 2.0f;
	if(Anim > 0.02f)
		Glow(Rect, R, NEON, Anim * (Hot ? 1.3f : 1.0f), IGraphics::CORNER_ALL, 1.0f, 3);
	ColorRGBA Track = Lerp(OFF, Hot ? ACCENT_HI : ACCENT, Anim);
	if(Hot && Anim < 0.5f)
		Track = Lerp(Track, HOVER, 0.5f);
	DrawFramed(Rect, Track, ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), R, IGraphics::CORNER_ALL, 0.4f);
	const float Knob = Rect.h - 4.0f;
	CUIRect KnobRect;
	KnobRect.w = Knob;
	KnobRect.h = Knob;
	KnobRect.y = Rect.y + 2.0f;
	KnobRect.x = Rect.x + 2.0f + (Rect.w - 4.0f - Knob) * std::clamp(Anim, 0.0f, 1.0f);
	const CExactRounding Exact;
	KnobRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), IGraphics::CORNER_ALL, Knob / 2.0f);
}

// One call for every chrome part of the old theme (button, scrollbar, edit box ...).
// Coloured shapes (red, green ...) keep their color, only grey ones get the new look.
inline void DrawChrome(const CUIRect *pRect, ColorRGBA Tint, bool Accent, int Corners, float Rounding, int Kind)
{
	if(!IsGrey(Tint) || Tint.r < 0.3f)
	{
		pRect->Draw(Tint, Corners, Rounding);
		return;
	}
	const float Fade = std::min(1.0f, Tint.a / 0.2f);
	switch(Kind)
	{
	case KIND_SCROLLBAR:
	{
		const CExactRounding Exact;
		const float R = std::min(Scaled(Rounding, *pRect), std::min(pRect->w, pRect->h) / 2.0f);
		if(Tint.a <= 0.3f)
			pRect->Draw(TRACK, Corners, R);
		else
			pRect->Draw(Accent ? ACCENT_HI : ColorRGBA(ACCENT.r, ACCENT.g, ACCENT.b, 0.9f), Corners, R);
		break;
	}
	case KIND_CHECKBOX:
		DrawFramed(*pRect, Accent ? ACCENT : OFF, LINE, Scaled(Rounding, *pRect), Corners);
		break;
	case KIND_EDIT:
	{
		const float R = Scaled(Rounding, *pRect);
		DrawFramed(*pRect, Accent ? Rgb(0x171120) : INPUT, Accent ? ColorRGBA(ACCENT_TEXT.r, ACCENT_TEXT.g, ACCENT_TEXT.b, 0.55f) : LINE, R, Corners);
		break;
	}
	default:
		DrawSecondary(*pRect, Accent, false, Corners, Rounding, Fade);
		break;
	}
}

} // namespace CCNeon

#endif
