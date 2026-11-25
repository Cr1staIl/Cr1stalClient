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

} // namespace CCStyle
