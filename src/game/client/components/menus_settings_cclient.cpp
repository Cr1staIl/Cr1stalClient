/* Cr1stal Client (C-Client) settings page. Based on DDNet, see licence.txt. */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>

static constexpr float CC_LINE = 20.0f;
static constexpr float CC_HEADLINE = 20.0f;
static constexpr float CC_SPACING = 5.0f;

static void CcHeadline(CUi *pUi, CUIRect *pColumn, const char *pText)
{
	CUIRect Label;
	pColumn->HSplitTop(CC_HEADLINE + 6.0f, &Label, pColumn);
	SLabelProperties Props;
	Props.SetColor(ColorRGBA(0.55f, 0.85f, 1.0f, 1.0f));
	pUi->DoLabel(&Label, pText, CC_HEADLINE * 0.8f, TEXTALIGN_ML, Props);
	pColumn->HSplitTop(2.0f, nullptr, pColumn);
}

void CMenus::RenderSettingsCClient(CUIRect MainView)
{
	CUIRect Left, Right, Button, Label;
	MainView.VSplitMid(&Left, &Right, 20.0f);

	// ---------------- left column ----------------

	// cursor
	CcHeadline(Ui(), &Left, Localize("Cursor"));

	// cursor distance: slider 0-400 plus a field to type any value
	{
		static CLineInputNumber s_MouseDistanceInput;
		static int s_LastValue = -1;
		if(!s_MouseDistanceInput.IsActive() && s_LastValue != g_Config.m_ClMouseMaxDistance)
		{
			s_MouseDistanceInput.SetInteger(g_Config.m_ClMouseMaxDistance);
			s_LastValue = g_Config.m_ClMouseMaxDistance;
		}

		Left.HSplitTop(CC_LINE, &Button, &Left);
		CUIRect Slider, Field;
		Button.VSplitRight(60.0f, &Slider, &Field);
		Slider.VSplitRight(5.0f, &Slider, nullptr);

		int SliderValue = std::min(g_Config.m_ClMouseMaxDistance, 400);
		if(Ui()->DoScrollbarOption(&g_Config.m_ClMouseMaxDistance, &SliderValue, &Slider, Localize("Cursor distance"), 0, 400))
		{
			g_Config.m_ClMouseMaxDistance = SliderValue;
			s_MouseDistanceInput.SetInteger(SliderValue);
			s_LastValue = SliderValue;
		}

		if(Ui()->DoEditBox(&s_MouseDistanceInput, &Field, 12.0f))
		{
			const int Typed = std::clamp(s_MouseDistanceInput.GetInteger(), 0, 5000);
			g_Config.m_ClMouseMaxDistance = Typed;
			s_LastValue = Typed;
		}
		Ui()->DoToolTip(&s_MouseDistanceInput, &Field, Localize("Type any value from 0 to 5000"));

		if(g_Config.m_ClDyncam)
		{
			Left.HSplitTop(CC_LINE, &Label, &Left);
			SLabelProperties Props;
			Props.SetColor(ColorRGBA(1.0f, 0.8f, 0.4f, 1.0f));
			Ui()->DoLabel(&Label, Localize("Dynamic camera is on: it uses its own distance"), 11.0f, TEXTALIGN_ML, Props);
		}
	}

	Left.HSplitTop(CC_LINE, &Button, &Left);
	Ui()->DoScrollbarOption(&g_Config.m_CcCursorScale, &g_Config.m_CcCursorScale, &Button, Localize("Cursor size"), 50, 300, &CUi::ms_LinearScrollbarScale, 0u, "%");

	Left.HSplitTop(CC_SPACING * 2, nullptr, &Left);

	// fast input
	CcHeadline(Ui(), &Left, Localize("Fast input"));
	Left.HSplitTop(CC_LINE, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_CcFastInput, Localize("Enable fast input"), g_Config.m_CcFastInput, &Button))
		g_Config.m_CcFastInput ^= 1;
	Ui()->DoToolTip(&g_Config.m_CcFastInput, &Button, Localize("Your tee reacts to your keys right away, before the server tick. Only changes what you see, not what the server gets."));
	if(g_Config.m_CcFastInput)
	{
		Left.HSplitTop(CC_LINE, &Button, &Left);
		Ui()->DoScrollbarOption(&g_Config.m_CcFastInputAmount, &g_Config.m_CcFastInputAmount, &Button, Localize("Amount"), 20, 100, &CUi::ms_LinearScrollbarScale, 0u, "ms");
	}

	Left.HSplitTop(CC_SPACING * 2, nullptr, &Left);

	// looks
	CcHeadline(Ui(), &Left, Localize("Looks"));
	Left.HSplitTop(CC_LINE, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_CrNeonBackground, Localize("Neon menu background"), g_Config.m_CrNeonBackground, &Button))
		g_Config.m_CrNeonBackground ^= 1;

	Left.HSplitTop(CC_LINE, &Button, &Left);
	if(DoButton_CheckBox(&g_Config.m_CcRainbowTee, Localize("Rainbow tee (only you see it)"), g_Config.m_CcRainbowTee, &Button))
		g_Config.m_CcRainbowTee ^= 1;
	if(g_Config.m_CcRainbowTee)
	{
		Left.HSplitTop(CC_LINE, &Button, &Left);
		Ui()->DoScrollbarOption(&g_Config.m_CcRainbowSpeed, &g_Config.m_CcRainbowSpeed, &Button, Localize("Rainbow speed"), 10, 500, &CUi::ms_LinearScrollbarScale, 0u, "%");
	}

	// ---------------- right column ----------------

	// antiping
	CcHeadline(Ui(), &Right, Localize("Antiping"));
	Right.HSplitTop(CC_LINE, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClAntiPing, Localize("Enable antiping"), g_Config.m_ClAntiPing, &Button))
		g_Config.m_ClAntiPing ^= 1;
	Ui()->DoToolTip(&g_Config.m_ClAntiPing, &Button, Localize("Predicts other players and projectiles so the game feels like a lower ping"));
	if(g_Config.m_ClAntiPing)
	{
		Right.HSplitTop(CC_LINE, &Button, &Right);
		const int PlayersOn = g_Config.m_ClAntiPingPlayers != 0;
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingPlayers, Localize("Predict other players"), PlayersOn, &Button))
			g_Config.m_ClAntiPingPlayers = PlayersOn ? 0 : 1;

		Right.HSplitTop(CC_LINE, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingSmooth, Localize("Smooth other players"), g_Config.m_ClAntiPingSmooth, &Button))
			g_Config.m_ClAntiPingSmooth ^= 1;

		Right.HSplitTop(CC_LINE, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingGrenade, Localize("Predict grenades"), g_Config.m_ClAntiPingGrenade, &Button))
			g_Config.m_ClAntiPingGrenade ^= 1;

		Right.HSplitTop(CC_LINE, &Button, &Right);
		if(DoButton_CheckBox(&g_Config.m_ClAntiPingWeapons, Localize("Predict weapons"), g_Config.m_ClAntiPingWeapons, &Button))
			g_Config.m_ClAntiPingWeapons ^= 1;

		Right.HSplitTop(CC_LINE, &Button, &Right);
		Ui()->DoScrollbarOption(&g_Config.m_ClAntiPingPercent, &g_Config.m_ClAntiPingPercent, &Button, Localize("Prediction strength"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
	}

	Right.HSplitTop(CC_LINE, &Button, &Right);
	const int FreezeOn = g_Config.m_ClPredictFreeze != 0;
	if(DoButton_CheckBox(&g_Config.m_ClPredictFreeze, Localize("Predict freeze"), FreezeOn, &Button))
		g_Config.m_ClPredictFreeze = FreezeOn ? 0 : 1;

	Right.HSplitTop(CC_SPACING * 2, nullptr, &Right);

	// team HUD
	CcHeadline(Ui(), &Right, Localize("Team"));
	Right.HSplitTop(CC_LINE, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_CcFrozenHud, Localize("Show team at the top (frozen tees darkened)"), g_Config.m_CcFrozenHud, &Button))
		g_Config.m_CcFrozenHud ^= 1;
	if(g_Config.m_CcFrozenHud)
	{
		Right.HSplitTop(CC_LINE, &Button, &Right);
		Ui()->DoScrollbarOption(&g_Config.m_CcFrozenHudSize, &g_Config.m_CcFrozenHudSize, &Button, Localize("Tee size"), 8, 24);
	}

	Right.HSplitTop(CC_LINE, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_CcFrozenText, Localize("Show alive / total counter"), g_Config.m_CcFrozenText, &Button))
		g_Config.m_CcFrozenText ^= 1;

	Right.HSplitTop(CC_LINE, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_CcFrozenHudTeamOnly, Localize("Only while in a team"), g_Config.m_CcFrozenHudTeamOnly, &Button))
		g_Config.m_CcFrozenHudTeamOnly ^= 1;

	Right.HSplitTop(CC_LINE, &Button, &Right);
	if(DoButton_CheckBox(&g_Config.m_ClShowFreezeBars, Localize("Freeze bars under frozen tees"), g_Config.m_ClShowFreezeBars, &Button))
		g_Config.m_ClShowFreezeBars ^= 1;
}
