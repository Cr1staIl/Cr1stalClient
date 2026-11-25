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

} // namespace CCStyle
