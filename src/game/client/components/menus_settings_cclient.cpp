/* Cr1stal Client (C-Client) settings page. Based on DDNet, see licence.txt. */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>

static constexpr float CC_LINE = 20.0f;
static constexpr float CC_HEADLINE = 20.0f;
static constexpr float CC_SECTION_GAP = 12.0f;

// Small helper that lays out one settings column from top to bottom.
class CCcColumn
{
public:
	CMenus *m_pMenus;
	CUi *m_pUi;
	CUIRect m_Rect;
	float m_StartY;

	CCcColumn(CMenus *pMenus, CUi *pUi, CUIRect Rect) :
		m_pMenus(pMenus), m_pUi(pUi), m_Rect(Rect), m_StartY(Rect.y) {}

	float Used() const { return m_Rect.y - m_StartY; }

	CUIRect Line(float Height = CC_LINE)
	{
		CUIRect Line;
		m_Rect.HSplitTop(Height, &Line, &m_Rect);
		return Line;
	}

	void Gap(float Height = CC_SECTION_GAP) { m_Rect.HSplitTop(Height, nullptr, &m_Rect); }

	void Headline(const char *pText)
	{
		CUIRect Label = Line(CC_HEADLINE + 6.0f);
		SLabelProperties Props;
		Props.SetColor(ColorRGBA(0.55f, 0.85f, 1.0f, 1.0f));
		m_pUi->DoLabel(&Label, pText, CC_HEADLINE * 0.8f, TEXTALIGN_ML, Props);
		Gap(2.0f);
	}

	// checkbox for a 0/1 setting
	void Check(int *pValue, const char *pText, const char *pTooltip = nullptr)
	{
		CUIRect Button = Line();
		if(m_pMenus->DoButton_CheckBox(pValue, pText, *pValue, &Button))
			*pValue ^= 1;
		if(pTooltip)
			m_pUi->DoToolTip(pValue, &Button, pTooltip);
	}

	// checkbox for a setting that has more values than on/off
	void CheckValue(int *pValue, const char *pText, int OnValue, int OffValue = 0)
	{
		CUIRect Button = Line();
		const bool On = *pValue != OffValue;
		if(m_pMenus->DoButton_CheckBox(pValue, pText, On, &Button))
			*pValue = On ? OffValue : OnValue;
	}

	void Slider(int *pValue, const char *pText, int Min, int Max, const char *pSuffix = "")
	{
		CUIRect Button = Line();
		m_pUi->DoScrollbarOption(pValue, pValue, &Button, pText, Min, Max, &CUi::ms_LinearScrollbarScale, 0u, pSuffix);
	}

	void Note(const char *pText, ColorRGBA Color = ColorRGBA(1.0f, 0.8f, 0.4f, 1.0f))
	{
		CUIRect Label = Line(16.0f);
		SLabelProperties Props;
		Props.SetColor(Color);
		m_pUi->DoLabel(&Label, pText, 10.0f, TEXTALIGN_ML, Props);
	}
};

void CMenus::RenderSettingsCClient(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	CUIRect LeftRect, RightRect;
	MainView.VSplitMid(&LeftRect, &RightRect, 20.0f);
	LeftRect.h = RightRect.h = 5000.0f; // columns grow downwards, the scroll region shows what fits
	CCcColumn Left(this, Ui(), LeftRect);
	CCcColumn Right(this, Ui(), RightRect);

	// ================= left column =================

	// ---- cursor ----
	Left.Headline(Localize("Cursor"));
	{
		// cursor distance: slider 0-400 plus a field to type any value
		static CLineInputNumber s_MouseDistanceInput;
		static int s_LastValue = -1;
		if(!s_MouseDistanceInput.IsActive() && s_LastValue != g_Config.m_ClMouseMaxDistance)
		{
			s_MouseDistanceInput.SetInteger(g_Config.m_ClMouseMaxDistance);
			s_LastValue = g_Config.m_ClMouseMaxDistance;
		}

		CUIRect Row = Left.Line();
		CUIRect SliderRect, Field;
		Row.VSplitRight(60.0f, &SliderRect, &Field);
		SliderRect.VSplitRight(5.0f, &SliderRect, nullptr);

		int SliderValue = std::min(g_Config.m_ClMouseMaxDistance, 400);
		if(Ui()->DoScrollbarOption(&g_Config.m_ClMouseMaxDistance, &SliderValue, &SliderRect, Localize("Cursor distance"), 0, 400))
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
			Left.Note(Localize("Dynamic camera is on: it uses its own distance"));
	}
	Left.Slider(&g_Config.m_CcCursorScale, Localize("Cursor size"), 50, 300, "%");
	Left.Check(&g_Config.m_CcShowCenter, Localize("Lines through the screen center"));
	Left.Gap();

	// ---- fast input ----
	Left.Headline(Localize("Fast input"));
	Left.Check(&g_Config.m_CcFastInput, Localize("Enable fast input"), Localize("Your tee reacts to your keys right away, before the server tick. Only changes what you see, not what the server gets."));
	if(g_Config.m_CcFastInput)
		Left.Slider(&g_Config.m_CcFastInputAmount, Localize("Amount"), 20, 100, "ms");
	Left.Gap();

	// ---- antiping ----
	Left.Headline(Localize("Antiping"));
	Left.Check(&g_Config.m_ClAntiPing, Localize("Enable antiping"), Localize("Predicts other players and projectiles so the game feels like a lower ping"));
	if(g_Config.m_ClAntiPing)
	{
		Left.CheckValue(&g_Config.m_ClAntiPingPlayers, Localize("Predict other players"), 1);
		Left.Check(&g_Config.m_ClAntiPingSmooth, Localize("Smooth other players"));
		Left.Check(&g_Config.m_ClAntiPingGrenade, Localize("Predict grenades"));
		Left.Check(&g_Config.m_ClAntiPingWeapons, Localize("Predict weapons"));
		Left.Slider(&g_Config.m_ClAntiPingPercent, Localize("Prediction strength"), 0, 100, "%");
	}
	Left.CheckValue(&g_Config.m_ClPredictFreeze, Localize("Predict freeze"), 1);
	Left.Gap();

	// ---- tees ----
	Left.Headline(Localize("Tees"));
	Left.Check(&g_Config.m_CcShowHitbox, Localize("Show real hitbox of tees"));
	Left.Slider(&g_Config.m_ClShowOthersAlpha, Localize("Tees of other teams"), 0, 100, "%");
	Left.Check(&g_Config.m_ClShowNinja, Localize("Ninja skin"));
	Left.Check(&g_Config.m_ClShowEmotes, Localize("Show emotes"));
	Left.CheckValue(&g_Config.m_ClShowDirection, Localize("Show pressed keys of all tees"), 2, 0);
	Left.CheckValue(&g_Config.m_ClShowHookCollOwn, Localize("Own hook line"), 1);
	Left.CheckValue(&g_Config.m_ClShowHookCollOther, Localize("Hook lines of others"), 1);
	Left.Gap();

	// ---- looks ----
	Left.Headline(Localize("Looks"));
	Left.Check(&g_Config.m_CrNeonBackground, Localize("Neon menu background"));
	Left.Check(&g_Config.m_CcRainbowTee, Localize("Rainbow tee (only you see it)"));
	if(g_Config.m_CcRainbowTee)
		Left.Slider(&g_Config.m_CcRainbowSpeed, Localize("Rainbow speed"), 10, 500, "%");
	Left.Check(&g_Config.m_CcTrail, Localize("Trail behind your tee"));
	if(g_Config.m_CcTrail)
	{
		Left.Check(&g_Config.m_CcTrailRainbow, Localize("Rainbow trail"));
		Left.Slider(&g_Config.m_CcTrailLength, Localize("Trail length"), 5, 120);
		Left.Slider(&g_Config.m_CcTrailWidth, Localize("Trail width"), 2, 40);
	}

	// ================= right column =================

	// ---- team ----
	Right.Headline(Localize("Team"));
	Right.Check(&g_Config.m_CcFrozenHud, Localize("Show team at the top (frozen tees darkened)"));
	if(g_Config.m_CcFrozenHud)
		Right.Slider(&g_Config.m_CcFrozenHudSize, Localize("Tee size"), 8, 24);
	Right.Check(&g_Config.m_CcFrozenText, Localize("Show alive / total counter"));
	Right.Check(&g_Config.m_CcFrozenHudTeamOnly, Localize("Only while in a team"));
	Right.Check(&g_Config.m_CcLastNotify, Localize("Warn when you are the last one alive"));
	Right.Check(&g_Config.m_ClShowFreezeBars, Localize("Freeze bars under frozen tees"));
	Right.Check(&g_Config.m_CcPlayerIndicator, Localize("Show teammates that are off screen"));
	if(g_Config.m_CcPlayerIndicator)
	{
		Right.Slider(&g_Config.m_CcIndicatorOffset, Localize("Indicator distance"), 50, 400);
		Right.Slider(&g_Config.m_CcIndicatorSize, Localize("Indicator size"), 4, 30);
		Right.Note(Localize("Green: alive, blue: frozen"), ColorRGBA(0.6f, 0.9f, 1.0f, 1.0f));
	}
	Right.Gap();

	// ---- HUD ----
	Right.Headline(Localize("HUD"));
	Right.Check(&g_Config.m_CcStatusbar, Localize("Status bar (FPS, ping, team, time)"));
	Right.Check(&g_Config.m_ClShowfps, Localize("Show FPS"));
	Right.Check(&g_Config.m_ClShowpred, Localize("Show prediction time"));
	Right.Check(&g_Config.m_ClShowLocalTimeAlways, Localize("Always show local time"));
	Right.Check(&g_Config.m_ClShowhudPlayerPosition, Localize("Show your position"));
	Right.Check(&g_Config.m_ClShowhudPlayerSpeed, Localize("Show your speed"));
	Right.Check(&g_Config.m_ClShowhudPlayerAngle, Localize("Show your aim angle"));
	Right.Check(&g_Config.m_ClShowhudJumpsIndicator, Localize("Show jumps you have left"));
	Right.Check(&g_Config.m_ClShowhudDummyActions, Localize("Show dummy actions"));
	Right.Slider(&g_Config.m_ClOverlayEntities, Localize("Game tiles over the map"), 0, 100, "%");
	Right.Gap();

	// ---- chat ----
	Right.Headline(Localize("Chat"));
	Right.Check(&g_Config.m_ClShowKillMessages, Localize("Show kill messages"));
	Right.Check(&g_Config.m_ClShowChatFriends, Localize("Only show chat of friends"));
	Right.Check(&g_Config.m_ClChatTeamColors, Localize("Names in team colors"));
	Right.Gap();

	// ---- automation that the game already supports ----
	Right.Headline(Localize("On join"));
	{
		static CLineInput s_RunOnJoinInput(g_Config.m_ClRunOnJoin, sizeof(g_Config.m_ClRunOnJoin));
		CUIRect Label = Right.Line(16.0f);
		Ui()->DoLabel(&Label, Localize("Console command when you join a server"), 11.0f, TEXTALIGN_ML);
		CUIRect Field = Right.Line();
		Ui()->DoClearableEditBox(&s_RunOnJoinInput, &Field, 12.0f);
	}

	// let the scroll region know how tall the content is
	CUIRect Content = MainView;
	Content.h = std::max(Left.Used(), Right.Used()) + 10.0f;
	s_ScrollRegion.AddRect(Content);
	s_ScrollRegion.End();
}
