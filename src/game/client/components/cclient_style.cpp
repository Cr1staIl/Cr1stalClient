/* Cr1stal Client: shared colors and shapes of the menu design. Based on DDNet, see licence.txt. */
#include "cclient_style.h"

#include <base/math.h>
#include <base/str.h>

#include <cmath>

namespace CCStyle
{

	std::string Upper(const char *pText)
	{
		std::string Result;
		const char *pCur = pText;
		while(*pCur)
		{
			int Code = str_utf8_decode(&pCur);
			if(Code < 0)
				break;
			if(Code >= 'a' && Code <= 'z')
				Code -= 'a' - 'A';
			else if(Code >= 0x430 && Code <= 0x44F) // а - я
				Code -= 0x20;
			else if(Code >= 0x450 && Code <= 0x45F) // ѐ - џ, ё
				Code -= 0x50;
			char aBuf[8];
			const int Len = str_utf8_encode(aBuf, Code);
			Result.append(aBuf, Len);
		}
		return Result;
	}

	void NeonGlow(const std::function<void(vec2 Offset, ColorRGBA Color)> &Draw, ColorRGBA Glow, float Strength, float Radius)
	{
		if(Strength <= 0.0f)
			return;
		// inner ring: 8 soft copies, outer ring: 6 fainter copies further out
		for(int i = 0; i < 8; i++)
		{
			const float Angle = i * pi / 4.0f;
			ColorRGBA Color = Glow;
			Color.a = Glow.a * 0.10f * Strength;
			Draw(vec2(std::cos(Angle), std::sin(Angle)) * Radius, Color);
		}
		for(int i = 0; i < 6; i++)
		{
			const float Angle = i * pi / 3.0f + pi / 6.0f;
			ColorRGBA Color = Glow;
			Color.a = Glow.a * 0.05f * Strength;
			Draw(vec2(std::cos(Angle), std::sin(Angle)) * Radius * 1.9f, Color);
		}
	}

	float NeonPulse(float Time, float Phase)
	{
		return 0.825f + 0.175f * std::sin(1.6f * Time + Phase);
	}

	void TexturedRoundRect(IGraphics *pGraphics, float X, float Y, float W, float H, float Rounding, ColorRGBA Color)
	{
		Rounding = std::clamp(Rounding, 0.0f, std::min(W, H) / 2.0f);
		// thin rows, each one as wide as the rounded shape is at its height
		constexpr int Rows = 24;
		auto Inset = [&](float LocalY) {
			float Dy = 0.0f;
			if(LocalY < Rounding)
				Dy = Rounding - LocalY;
			else if(LocalY > H - Rounding)
				Dy = LocalY - (H - Rounding);
			else
				return 0.0f;
			return Rounding - std::sqrt(std::max(0.0f, Rounding * Rounding - Dy * Dy));
		};
		pGraphics->QuadsBegin();
		pGraphics->SetColor(Color);
		for(int i = 0; i < Rows; i++)
		{
			const float Y0 = H * i / Rows, Y1 = H * (i + 1) / Rows;
			const float I0 = Inset(Y0), I1 = Inset(Y1);
			pGraphics->QuadsSetSubsetFree(
				I0 / W, Y0 / H, 1.0f - I0 / W, Y0 / H,
				I1 / W, Y1 / H, 1.0f - I1 / W, Y1 / H);
			const IGraphics::CFreeformItem Item(
				X + I0, Y + Y0, X + W - I0, Y + Y0,
				X + I1, Y + Y1, X + W - I1, Y + Y1);
			pGraphics->QuadsDrawFreeform(&Item, 1);
		}
		pGraphics->QuadsEnd();
	}

	void ApplyTheme(int Theme)
	{
		// the main color of each theme, the other shades are made from it
		static const ColorRGBA s_aBase[NUM_THEMES] = {
			ColorRGBA(0.86f, 0.07f, 0.23f, 1.0f), // crimson
			ColorRGBA(0.10f, 0.45f, 0.95f, 1.0f), // ocean
			ColorRGBA(0.58f, 0.20f, 0.95f, 1.0f), // violet
			ColorRGBA(0.10f, 0.78f, 0.45f, 1.0f), // emerald
			ColorRGBA(0.95f, 0.66f, 0.10f, 1.0f), // gold
			ColorRGBA(0.45f, 0.85f, 1.0f, 1.0f), // ice
		};
		const ColorRGBA Base = s_aBase[std::clamp(Theme, 0, NUM_THEMES - 1)];
		ACCENT = Base.WithAlpha(0.92f);
		ACCENT_SOFT = Base.WithAlpha(0.22f);
		ACCENT_TEXT = Mix(Base, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.15f);
		ACCENT_DEEP = ColorRGBA(Base.r * 0.92f, Base.g * 0.6f, Base.b * 0.65f, 1.0f);
	}

} // namespace CCStyle
