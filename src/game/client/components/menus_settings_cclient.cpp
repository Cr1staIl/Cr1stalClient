/* Cr1stal Client (C-Client) settings page. Based on DDNet, see licence.txt. */
#include "menus.h"

#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/cclient.h>
#include <game/client/components/cclient_style.h>
#include <game/client/gameclient.h>
#include <game/client/lineinput.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <map>
#include <vector>

static constexpr float CC_LINE = 20.0f;
static constexpr float CC_TOGGLE_LINE = 24.0f;
static constexpr float CC_CARD_PAD = 12.0f;
static constexpr float CC_CARD_GAP = 12.0f;
static constexpr int CC_MAX_CARDS = 32;

// card heights of the last frame: the card is drawn before its content, so it uses the
// height the content had one frame ago (nobody can see a one frame delay)
static float s_aCardHeights[CC_MAX_CARDS] = {};
static int s_NextColorReset = 0;

// Lays out one settings column from top to bottom, every group of settings in its own card.
class CCcColumn
{
public:
	CMenus *m_pMenus;
	CUi *m_pUi;
	CUIRect m_Rect;
	float m_StartY;

	int m_CardIndex = -1;
	float m_CardStartY = 0.0f;
	float m_OuterX = 0.0f;
	float m_OuterW = 0.0f;

	CCcColumn(CMenus *pMenus, CUi *pUi, CUIRect Rect) :
		m_pMenus(pMenus), m_pUi(pUi), m_Rect(Rect), m_StartY(Rect.y) {}

	float Used() const { return m_Rect.y - m_StartY; }

	CUIRect Line(float Height = CC_LINE)
	{
		CUIRect Line;
		m_Rect.HSplitTop(Height, &Line, &m_Rect);
		return Line;
	}

	void Gap(float Height = 6.0f) { m_Rect.HSplitTop(Height, nullptr, &m_Rect); }

	void BeginCard(int Index, const char *pTitle)
	{
		m_CardIndex = Index;
		m_CardStartY = m_Rect.y;
		if(s_aCardHeights[Index] > 0.0f)
		{
			CUIRect Card;
			Card.x = m_Rect.x;
			Card.y = m_Rect.y;
			Card.w = m_Rect.w;
			Card.h = s_aCardHeights[Index];
			// the card under the mouse gets a soft red edge
			const float Glow = m_pUi->StateAnim(&s_aCardHeights[Index], m_pUi->MouseHovered(&Card), 8.0f);
			CCStyle::DrawCard(Card, CCStyle::CARD_ROUNDING, CCStyle::CARD, Glow * 0.45f);
		}
		m_OuterX = m_Rect.x;
		m_OuterW = m_Rect.w;
		m_Rect.x += CC_CARD_PAD;
		m_Rect.w -= CC_CARD_PAD * 2.0f;

		Gap(10.0f);
		CUIRect Label = Line(18.0f);
		SLabelProperties Props;
		Props.SetColor(CCStyle::ACCENT_TEXT);
		m_pUi->DoLabel(&Label, CCStyle::Upper(pTitle).c_str(), 12.5f, TEXTALIGN_ML, Props);
		Gap(4.0f);
	}

	void EndCard()
	{
		Gap(10.0f);
		s_aCardHeights[m_CardIndex] = m_Rect.y - m_CardStartY;
		m_Rect.x = m_OuterX;
		m_Rect.w = m_OuterW;
		Gap(CC_CARD_GAP);
	}

	// one settings row: text on the left, iPhone style switch on the right.
	// returns true when the row was clicked
	bool Toggle(const void *pId, const char *pText, bool On, const char *pTooltip = nullptr)
	{
		CUIRect Row = Line(CC_TOGGLE_LINE);

		// smooth knob movement, 0 = off, 1 = on
		static std::map<const void *, float> s_Anim;
		auto It = s_Anim.find(pId);
		if(It == s_Anim.end())
			It = s_Anim.emplace(pId, On ? 1.0f : 0.0f).first;
		const float Target = On ? 1.0f : 0.0f;
		const float Step = std::min(1.0f, m_pMenus->Client()->RenderFrameTime() * 9.0f);
		It->second += (Target - It->second) * Step;
		if(absolute(It->second - Target) < 0.002f)
			It->second = Target;
		const float A = It->second;

		const float HoverA = m_pUi->HoverAnim(pId);
		if(HoverA > 0.01f)
		{
			CUIRect Hover = Row;
			Hover.VMargin(-4.0f, &Hover);
			Hover.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.045f * HoverA), IGraphics::CORNER_ALL, 6.0f);
		}

		CUIRect Rest, Switch;
		Row.VSplitRight(36.0f, &Rest, &Switch);
		Switch.HMargin((Switch.h - 18.0f) / 2.0f, &Switch);
		const ColorRGBA Track(
			CCStyle::TOGGLE_OFF.r + (CCStyle::TOGGLE_ON.r - CCStyle::TOGGLE_OFF.r) * A,
			CCStyle::TOGGLE_OFF.g + (CCStyle::TOGGLE_ON.g - CCStyle::TOGGLE_OFF.g) * A,
			CCStyle::TOGGLE_OFF.b + (CCStyle::TOGGLE_ON.b - CCStyle::TOGGLE_OFF.b) * A, 1.0f);
		Switch.Draw(Track, IGraphics::CORNER_ALL, Switch.h / 2.0f);

		const float KnobSize = Switch.h - 3.0f;
		CUIRect Knob;
		Knob.x = Switch.x + 1.5f + (Switch.w - KnobSize - 3.0f) * A;
		Knob.y = Switch.y + 1.5f;
		Knob.w = Knob.h = KnobSize;
		CUIRect Shadow = Knob;
		Shadow.y += 0.8f;
		Shadow.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, KnobSize / 2.0f);
		Knob.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), IGraphics::CORNER_ALL, KnobSize / 2.0f);

		CUIRect Label;
		Rest.VSplitRight(8.0f, &Label, nullptr);
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Props.SetColor(CCStyle::TEXT);
		m_pUi->DoLabel(&Label, pText, 12.0f, TEXTALIGN_ML, Props);

		// thin divider like the iPhone settings lists
		CUIRect Divider = Row;
		Divider.y = Row.y + Row.h - 0.5f;
		Divider.h = 0.5f;
		Divider.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_NONE, 0.0f);

		if(pTooltip)
			m_pUi->DoToolTip(pId, &Row, pTooltip);
		return m_pUi->DoButtonLogic(pId, 0, &Row, BUTTONFLAG_LEFT) != 0;
	}

	// switch for a 0/1 setting
	void Check(int *pValue, const char *pText, const char *pTooltip = nullptr)
	{
		if(Toggle(pValue, pText, *pValue != 0, pTooltip))
			*pValue ^= 1;
	}

	// switch for a setting that has more values than on/off
	void CheckValue(int *pValue, const char *pText, int OnValue, int OffValue = 0)
	{
		const bool On = *pValue != OffValue;
		if(Toggle(pValue, pText, On))
			*pValue = On ? OffValue : OnValue;
	}

	void Slider(int *pValue, const char *pText, int Min, int Max, const char *pSuffix = "")
	{
		Gap(2.0f);
		CUIRect Button = Line();
		m_pUi->DoScrollbarOption(pValue, pValue, &Button, pText, Min, Max, &CUi::ms_LinearScrollbarScale, 0u, pSuffix);
		Gap(2.0f);
	}

	void Color(const char *pText, unsigned *pValue, unsigned DefaultValue)
	{
		static CButtonContainer s_aResetButtons[16];
		CButtonContainer *pReset = &s_aResetButtons[s_NextColorReset++ % 16];
		m_pMenus->DoLine_ColorPicker(pReset, CC_LINE, 12.0f, 3.0f, &m_Rect, pText, pValue, color_cast<ColorRGBA>(ColorHSLA(DefaultValue, true)), false, nullptr, true);
	}

	// small gray caption above a control
	void Caption(const char *pText)
	{
		CUIRect Label = Line(16.0f);
		SLabelProperties Props;
		Props.SetColor(CCStyle::TEXT_DIM);
		m_pUi->DoLabel(&Label, pText, 11.0f, TEXTALIGN_ML, Props);
	}

	// iPhone style segmented control: caption above, one rounded track, the chosen part highlighted
	void Choice(int *pValue, const char *pLabel, const char *const *ppOptions, int NumOptions, CButtonContainer *pButtons)
	{
		Gap(2.0f);
		Caption(pLabel);
		CUIRect Row = Line(22.0f);
		Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_ALL, 7.0f);
		Row.Margin(2.0f, &Row);
		const float ButtonW = Row.w / NumOptions;
		for(int i = 0; i < NumOptions; i++)
		{
			CUIRect Button;
			Row.VSplitLeft(ButtonW, &Button, &Row);
			const bool Selected = *pValue == i;
			// the chosen part fades in, the old one fades out
			static std::map<const void *, char> s_SelectKeys; // own address per button for the selection fade
			const float SelectA = m_pUi->StateAnim(&s_SelectKeys[&pButtons[i]], Selected);
			const float HoverA = m_pUi->HoverAnim(&pButtons[i]);
			if(HoverA > 0.01f)
				Button.Draw(CCStyle::HOVER.WithMultipliedAlpha(HoverA), IGraphics::CORNER_ALL, 5.5f);
			if(SelectA > 0.01f)
				Button.Draw(CCStyle::ACCENT.WithMultipliedAlpha(SelectA), IGraphics::CORNER_ALL, 5.5f);
			SLabelProperties Props;
			Props.m_MaxWidth = Button.w - 4.0f;
			Props.m_EllipsisAtEnd = true;
			Props.SetColor(CCStyle::Mix(CCStyle::TEXT_DIM, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), SelectA));
			m_pUi->DoLabel(&Button, ppOptions[i], 11.0f, TEXTALIGN_MC, Props);
			if(m_pUi->DoButtonLogic(&pButtons[i], 0, &Button, BUTTONFLAG_LEFT))
				*pValue = i;
		}
		Gap(4.0f);
	}

	// full width rounded button
	bool Button(const void *pId, const char *pText)
	{
		Gap(2.0f);
		CUIRect Rect = Line(22.0f);
		const float HoverA = m_pUi->HoverAnim(pId);
		Rect.Draw(CCStyle::Mix(CCStyle::BUTTON, CCStyle::ACCENT.WithAlpha(0.55f), HoverA), IGraphics::CORNER_ALL, Rect.h / 2.0f);
		SLabelProperties Props;
		Props.m_MaxWidth = Rect.w - 10.0f;
		Props.m_EllipsisAtEnd = true;
		Props.SetColor(CCStyle::TEXT);
		m_pUi->DoLabel(&Rect, pText, 11.5f, TEXTALIGN_MC, Props);
		const bool Clicked = m_pUi->DoButtonLogic(pId, 0, &Rect, BUTTONFLAG_LEFT) != 0;
		Gap(2.0f);
		return Clicked;
	}

	// caption and a text field
	void Edit(CLineInput *pInput, const char *pCaption)
	{
		Gap(2.0f);
		Caption(pCaption);
		CUIRect Field = Line();
		m_pUi->DoClearableEditBox(pInput, &Field, 12.0f);
		Gap(2.0f);
	}

	void Note(const char *pText, ColorRGBA Color = CCStyle::TEXT_DIM)
	{
		CUIRect Label = Line(16.0f);
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Props.SetColor(Color);
		m_pUi->DoLabel(&Label, pText, 10.5f, TEXTALIGN_ML, Props);
	}
};

enum
{
	CARD_INPUT = 0,
	CARD_ANTIPING,
	CARD_CURSOR,
	CARD_CUSTOMIZATION,
	CARD_TEES,
	CARD_LOOKS,
	CARD_OUTLINES,
	CARD_ISLAND,
	CARD_TEAM,
	CARD_HUD,
	CARD_SAVES,
	CARD_CHAT,
	CARD_UPDATES,
	CARD_ON_JOIN,
	CARD_PERF,
	CARD_AUTOMATION,
	CARD_GHOSTS,
	CARD_FUN,
	CARD_EFFECTS,
	CARD_GAMEPLAY,
	CARD_CONFIGS,
	NUM_CARDS,
};

// C-Client: whole configs saved under a name in cclient_configs/
struct SCcConfigFile
{
	std::string m_Name;
	time_t m_Time;
};

static int CcConfigScan(const CFsFileInfo *pInfo, int IsDir, int DirType, void *pUser)
{
	auto *pList = static_cast<std::vector<SCcConfigFile> *>(pUser);
	if(IsDir || !str_endswith(pInfo->m_pName, ".cfg"))
		return 0;
	char aName[128];
	str_truncate(aName, sizeof(aName), pInfo->m_pName, str_length(pInfo->m_pName) - 4);
	pList->push_back({aName, pInfo->m_TimeModified});
	return 0;
}

// copy a file in the save folder
static bool CcCopyFile(IStorage *pStorage, const char *pFrom, const char *pTo)
{
	void *pData = nullptr;
	unsigned Size = 0;
	if(!pStorage->ReadFile(pFrom, IStorage::TYPE_SAVE, &pData, &Size))
		return false;
	IOHANDLE File = pStorage->OpenFile(pTo, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	bool Ok = false;
	if(File)
	{
		Ok = io_write(File, pData, Size) == Size;
		io_close(File);
	}
	free(pData);
	return Ok;
}

// C-Client: the key a command is bound to (first match), or none
static CBindSlot CcFindBind(CBinds &Binds, const char *pCommand)
{
	for(int Mod = 0; Mod < KeyModifier::COMBINATION_COUNT; Mod++)
		for(int Key = KEY_FIRST; Key < KEY_LAST; Key++)
		{
			const char *pBind = Binds.Get(Key, Mod);
			if(pBind[0] && str_comp(pBind, pCommand) == 0)
				return CBindSlot(Key, Mod);
		}
	return EMPTY_BIND_SLOT;
}

static void CcKeyRow(CUi *pUi, CGameClient *pGameClient, CUIRect Row, const char *pLabel, const char *pCommand, CButtonContainer *pReader, CButtonContainer *pClear)
{
	const CBindSlot Current = CcFindBind(pGameClient->m_Binds, pCommand);
	CUIRect Label, Reader;
	Row.VSplitRight(110.0f, &Label, &Reader);
	pUi->DoLabel(&Label, pLabel, 12.0f, TEXTALIGN_ML);
	const CKeyBinder::CKeyReaderResult Result = pGameClient->m_KeyBinder.DoKeyReader(pReader, pClear, &Reader, Current, false);
	if(!Result.m_Aborted && Result.m_Bind != Current)
	{
		if(Current.m_Key != KEY_UNKNOWN)
			pGameClient->m_Binds.Bind(Current.m_Key, "", false, Current.m_ModifierMask);
		if(Result.m_Bind.m_Key != KEY_UNKNOWN)
			pGameClient->m_Binds.Bind(Result.m_Bind.m_Key, pCommand, false, Result.m_Bind.m_ModifierMask);
	}
}

void CMenus::RenderSettingsCClient(CUIRect MainView)
{
	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);
	s_NextColorReset = 0;

	// three columns of cards, two on narrow screens
	const float ColumnGap = 14.0f;
	const int NumColumns = MainView.w >= 760.0f ? 3 : 2;
	const float ColumnW = (MainView.w - ColumnGap * (NumColumns - 1)) / NumColumns;
	std::vector<CCcColumn> vColumns;
	for(int i = 0; i < NumColumns; i++)
	{
		CUIRect Rect = MainView;
		Rect.x = MainView.x + i * (ColumnW + ColumnGap);
		Rect.w = ColumnW;
		Rect.h = 5000.0f; // columns grow downwards, the scroll region shows what fits
		vColumns.emplace_back(this, Ui(), Rect);
	}
	// which column every card goes to, for 3 and for 2 columns
	static const int s_aColumnOf3[NUM_CARDS] = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 1, 0, 2};
	static const int s_aColumnOf2[NUM_CARDS] = {0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 1};
	auto Col = [&](int Card) -> CCcColumn & {
		return vColumns[NumColumns == 3 ? s_aColumnOf3[Card] : s_aColumnOf2[Card]];
	};

	// ---- input modes ----
	{
		CCcColumn &C = Col(CARD_INPUT);
		C.BeginCard(CARD_INPUT, Localize("Input"));
		static CButtonContainer s_aInputButtons[4];
		const char *apModes[] = {Localize("Off"), "Fast", "Sheylun", "Delta"};
		C.Choice(&g_Config.m_CcFastInput, Localize("Input mode"), apModes, 4, s_aInputButtons);
		switch(g_Config.m_CcFastInput)
		{
		case 1:
			C.Note(Localize("Fast Input: your tee reacts before the server tick"));
			C.Slider(&g_Config.m_CcFastInputAmount, Localize("Amount"), 20, 100, "ms");
			break;
		case 2:
			C.Note(Localize("Sheylun Input: predicts whole ticks, as many as your ping needs"));
			C.Slider(&g_Config.m_CcSheylunStrength, Localize("Strength"), 25, 200, "%");
			C.Slider(&g_Config.m_CcSheylunMaxTicks, Localize("Max ticks"), 1, 6);
			C.Check(&g_Config.m_CcSheylunTickAim, Localize("Aim updates once per tick (calmer hook line)"));
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Ticks right now"), GameClient()->m_Controls.FastInputTicks());
				C.Note(aBuf, ColorRGBA(0.55f, 0.9f, 0.65f, 1.0f));
			}
			break;
		case 3:
			C.Note(Localize("Delta Input: Fast Input plus finer aim for small mouse moves"));
			C.Slider(&g_Config.m_CcFastInputAmount, Localize("Amount"), 20, 100, "ms");
			C.Slider(&g_Config.m_CcDeltaPrecision, Localize("Small move sensitivity"), 20, 200, "%");
			C.Slider(&g_Config.m_CcDeltaThreshold, Localize("Small move size"), 1, 30);
			break;
		default:
			C.Note(Localize("Normal DDNet input"));
		}
		C.EndCard();
	}

	// ---- antiping ----
	{
		CCcColumn &C = Col(CARD_ANTIPING);
		C.BeginCard(CARD_ANTIPING, Localize("Antiping"));
		{
			// one click for gores: other tees, your hook and your tee all in the same moment
			static CButtonContainer s_GoresPreset;
			if(C.Button(&s_GoresPreset, Localize("Best for saving teammates")))
			{
				g_Config.m_ClAntiPing = 1;
				g_Config.m_ClAntiPingPlayers = 1;
				g_Config.m_ClAntiPingPercent = 100;
				g_Config.m_ClAntiPingPreInput = 1;
				g_Config.m_CcFastInputOthers = 1;
				// with fast input the DDNet smoothing works on the future tick too, else TClient's
				g_Config.m_ClAntiPingSmooth = g_Config.m_CcFastInput ? 1 : 0;
				g_Config.m_CcAntiPingImproved = g_Config.m_CcFastInput ? 0 : 1;
				g_Config.m_CcOthersEase = 0;
				g_Config.m_CcAntiPingStableDirection = 1;
				g_Config.m_CcUnpredOthersInFreeze = 0;
				g_Config.m_ClPredictionMargin = 10;
			}
			C.Note(Localize("Without antiping other tees are drawn where they were a ping ago, so hooking them feels late"));
		}
		C.Check(&g_Config.m_ClAntiPing, Localize("Enable antiping"), Localize("Predicts other players and projectiles so the game feels like a lower ping"));
		if(g_Config.m_ClAntiPing)
		{
			C.CheckValue(&g_Config.m_ClAntiPingPlayers, Localize("Predict other players"), 1);
			C.Check(&g_Config.m_ClAntiPingPreInput, Localize("Use the inputs others send early"));
			if(g_Config.m_CcFastInput)
				C.Check(&g_Config.m_CcFastInputOthers, Localize("Fast input for other tees too"), Localize("Others are shown on the same future tick as you, so saving has no delay"));
			C.Check(&g_Config.m_ClAntiPingSmooth, Localize("Smooth other players"));
			C.Check(&g_Config.m_ClAntiPingGrenade, Localize("Predict grenades"));
			C.Check(&g_Config.m_ClAntiPingWeapons, Localize("Predict weapons"));
			C.Slider(&g_Config.m_ClAntiPingPercent, Localize("Prediction strength"), 0, 100, "%");
		}
		C.CheckValue(&g_Config.m_ClPredictFreeze, Localize("Predict freeze"), 1);
		C.Slider(&g_Config.m_ClPredictionMargin, Localize("Prediction margin"), 1, 100, "ms");
		C.Slider(&g_Config.m_CcOthersEase, Localize("Smooth other tees"), 0, 100, "%");
		C.Gap(4.0f);
		C.Caption(Localize("Smooth antiping (TClient)"));
		C.Check(&g_Config.m_CcAntiPingImproved, Localize("New antiping smoothing"), Localize("Trusts the prediction of other tees only as far as they moved steadily before"));
		if(g_Config.m_CcAntiPingImproved)
		{
			C.Check(&g_Config.m_CcAntiPingStableDirection, Localize("Optimistic along the stable direction"));
			C.Check(&g_Config.m_CcAntiPingNegativeBuffer, Localize("Remember instability for longer"));
			C.Slider(&g_Config.m_CcAntiPingUncertaintyScale, Localize("Uncertainty duration"), 25, 400, "%");
		}
		C.Gap(4.0f);
		C.Caption(Localize("While frozen"));
		C.Check(&g_Config.m_CcRemoveAnti, Localize("Less prediction of your tee in freeze"));
		if(g_Config.m_CcRemoveAnti)
		{
			C.Slider(&g_Config.m_CcUnfreezeLagTicks, Localize("Ticks taken away"), 0, 20);
			C.Slider(&g_Config.m_CcUnfreezeLagDelayTicks, Localize("Ticks until all is taken away"), 5, 150);
		}
		C.Check(&g_Config.m_CcUnpredOthersInFreeze, Localize("Don't predict others while frozen"));
		C.Check(&g_Config.m_CcPredMarginInFreeze, Localize("Other prediction margin while frozen"));
		if(g_Config.m_CcPredMarginInFreeze)
			C.Slider(&g_Config.m_CcPredMarginInFreezeAmount, Localize("Margin while frozen"), 0, 100, "ms");
		C.EndCard();
	}

	// ---- cursor ----
	{
		CCcColumn &C = Col(CARD_CURSOR);
		C.BeginCard(CARD_CURSOR, Localize("Cursor"));
		// cursor distance: slider 0-400 plus a field to type any value
		static CLineInputNumber s_MouseDistanceInput;
		static int s_LastValue = -1;
		if(!s_MouseDistanceInput.IsActive() && s_LastValue != g_Config.m_ClMouseMaxDistance)
		{
			s_MouseDistanceInput.SetInteger(g_Config.m_ClMouseMaxDistance);
			s_LastValue = g_Config.m_ClMouseMaxDistance;
		}

		C.Gap(2.0f);
		CUIRect Row = C.Line();
		CUIRect SliderRect, Field;
		Row.VSplitRight(52.0f, &SliderRect, &Field);
		SliderRect.VSplitRight(6.0f, &SliderRect, nullptr);

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
		C.Gap(2.0f);
		if(g_Config.m_ClDyncam)
			C.Note(Localize("Dynamic camera is on: it uses its own distance"), ColorRGBA(1.0f, 0.82f, 0.5f, 1.0f));
		C.Slider(&g_Config.m_CcCursorScale, Localize("Cursor size"), 50, 300, "%");
		C.Check(&g_Config.m_CcShowCenter, Localize("Lines through the screen center"));
		C.EndCard();
	}

	// ---- customization ----
	{
		CCcColumn &C = Col(CARD_CUSTOMIZATION);
		C.BeginCard(CARD_CUSTOMIZATION, Localize("Customization"));
		{
			static CButtonContainer s_aThemes[CCStyle::NUM_THEMES];
			const char *apThemes[] = {Localize("Crimson"), Localize("Ocean"), Localize("Violet"), Localize("Emerald"), Localize("Gold"), Localize("Ice")};
			C.Choice(&g_Config.m_CcTheme, Localize("Theme"), apThemes, CCStyle::NUM_THEMES, s_aThemes);
		}
		{
			// font for all text in the game: < name >
			C.Gap(2.0f);
			C.Caption(Localize("Font"));
			CUIRect Row = C.Line(22.0f);
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_ALL, 7.0f);
			CUIRect Prev, Next, Name;
			Row.VSplitLeft(26.0f, &Prev, &Row);
			Row.VSplitRight(26.0f, &Name, &Next);
			static CButtonContainer s_PrevFont, s_NextFont;
			for(int Side = 0; Side < 2; Side++)
			{
				CButtonContainer *pButton = Side == 0 ? &s_PrevFont : &s_NextFont;
				CUIRect Arrow = Side == 0 ? Prev : Next;
				Arrow.Margin(2.0f, &Arrow);
				const float HoverA = Ui()->HoverAnim(pButton);
				if(HoverA > 0.01f)
					Arrow.Draw(CCStyle::ACCENT.WithAlpha(0.6f * HoverA), IGraphics::CORNER_ALL, 5.5f);
				Ui()->DoLabel(&Arrow, Side == 0 ? "<" : ">", 12.0f, TEXTALIGN_MC);
				if(Ui()->DoButtonLogic(pButton, 0, &Arrow, BUTTONFLAG_LEFT))
					g_Config.m_CcFont = (g_Config.m_CcFont + (Side == 0 ? CCClient::NUM_FONTS - 1 : 1)) % CCClient::NUM_FONTS;
			}
			Ui()->DoLabel(&Name, CCClient::FontDisplayName(g_Config.m_CcFont), 12.0f, TEXTALIGN_MC);
			C.Gap(2.0f);
		}
		{
			static CButtonContainer s_aHammerButtons[3];
			const char *apHammer[] = {Localize("Normal"), Localize("Follows cursor"), Localize("Like a gun")};
			C.Choice(&g_Config.m_CcHammerMode, Localize("Hammer"), apHammer, 3, s_aHammerButtons);
		}
		{
			static CButtonContainer s_aAuraButtons[CCClient::NUM_AURAS + 1];
			const char *apAura[] = {Localize("Off"), "1", "2", "3", "4"};
			C.Choice(&g_Config.m_CcAura, Localize("Picture behind tee"), apAura, CCClient::NUM_AURAS + 1, s_aAuraButtons);
			if(g_Config.m_CcAura)
			{
				C.Slider(&g_Config.m_CcAuraSize, Localize("Picture size"), 80, 600);
				C.Slider(&g_Config.m_CcAuraAlpha, Localize("Picture opacity"), 5, 100, "%");
				C.Slider(&g_Config.m_CcAuraSpin, Localize("Picture rotation"), 0, 300);
			}
		}
		C.EndCard();
	}

	// ---- tees ----
	{
		CCcColumn &C = Col(CARD_TEES);
		C.BeginCard(CARD_TEES, Localize("Tees"));
		C.Check(&g_Config.m_ClNamePlatesOwn, Localize("Show my own name"), Localize("Your name above your own tee"));
		C.Check(&g_Config.m_CcShowHitbox, Localize("Show real hitbox of tees"));
		C.Check(&g_Config.m_CcNameplatePingCircle, Localize("Ping colored circle next to names"));
		C.Check(&g_Config.m_CcNameplateCountry, Localize("Country flag next to names"));
		C.Check(&g_Config.m_CcNameplateSkins, Localize("Skin name under names"));
		C.Check(&g_Config.m_ClFreezeStars, Localize("Stars over frozen tees"));
		C.Check(&g_Config.m_CcColorFreeze, Localize("Frozen tees keep their color"));
		if(g_Config.m_CcColorFreeze)
		{
			C.Slider(&g_Config.m_CcColorFreezeDarken, Localize("Darker when frozen"), 0, 100, "%");
			C.Check(&g_Config.m_CcColorFreezeFeet, Localize("Also color the feet"));
		}
		C.Check(&g_Config.m_CcFrozenKatana, Localize("Katana in the hand of frozen tees"));
		C.CheckValue(&g_Config.m_CcRenderWeaponsAsGun, Localize("All weapons look like the gun"), 1);
		C.Check(&g_Config.m_CcWhiteFeet, Localize("White feet for custom colored tees"));
		if(g_Config.m_CcWhiteFeet)
		{
			static CLineInput s_WhiteFeetInput(g_Config.m_CcWhiteFeetSkin, sizeof(g_Config.m_CcWhiteFeetSkin));
			C.Edit(&s_WhiteFeetInput, Localize("Skin for the white feet"));
		}
		{
			static CButtonContainer s_aTinyButtons[3];
			int Tiny = g_Config.m_CcTinyTees ? (g_Config.m_CcTinyTeesOthers ? 2 : 1) : 0;
			const char *apTiny[] = {Localize("Off"), Localize("Own"), Localize("All")};
			C.Choice(&Tiny, Localize("Small tees"), apTiny, 3, s_aTinyButtons);
			g_Config.m_CcTinyTees = Tiny > 0;
			g_Config.m_CcTinyTeesOthers = Tiny > 1;
			if(Tiny)
				C.Slider(&g_Config.m_CcTinyTeeSize, Localize("Size of small tees"), 85, 115, "%");
		}
		{
			static CButtonContainer s_aFlagButtons[3];
			const char *apFlags[] = {Localize("Off"), Localize("Red"), Localize("Blue")};
			C.Choice(&g_Config.m_CcFakeCtfFlags, Localize("Fake CTF flags"), apFlags, 3, s_aFlagButtons);
		}
		C.Check(&g_Config.m_ClShowNinja, Localize("Ninja skin"));
		C.Check(&g_Config.m_ClShowEmotes, Localize("Show emotes"));
		C.CheckValue(&g_Config.m_ClShowDirection, Localize("Show pressed keys of all tees"), 2, 0);
		C.CheckValue(&g_Config.m_ClShowHookCollOwn, Localize("Own hook line"), 1);
		C.CheckValue(&g_Config.m_ClShowHookCollOther, Localize("Hook lines of others"), 1);
		C.Check(&g_Config.m_CcMovingTilesEntities, Localize("Moving tiles in entities"));
		C.Slider(&g_Config.m_ClShowOthersAlpha, Localize("Tees of other teams"), 0, 100, "%");
		C.EndCard();
	}

	// ---- looks ----
	{
		CCcColumn &C = Col(CARD_LOOKS);
		C.BeginCard(CARD_LOOKS, Localize("Looks"));
		C.Check(&g_Config.m_CrNeonBackground, Localize("Neon menu background"));
		C.Check(&g_Config.m_CcMenuSymbols, Localize("Floating signs in the menu background"));
		C.Gap(4.0f);
		C.Caption(Localize("Cr1stal players"));
		C.Check(&g_Config.m_CcClientBadge, Localize("Client avatar before their names"), Localize("In the scoreboard and above the tees"));
		C.Check(&g_Config.m_CcOthersPacks, Localize("See their texture packs"), Localize("Their hook and weapons are drawn with their own game pack"));
		C.Check(&g_Config.m_CcSharePresence, Localize("Show that I use Cr1stal Client"), Localize("Others with Cr1stal Client see your avatar and your game pack"));
		C.Gap(4.0f);
		C.Caption(Localize("Rainbow"));
		C.Check(&g_Config.m_CcRainbowTee, Localize("Rainbow tee"));
		C.Check(&g_Config.m_CcRainbowHook, Localize("Rainbow hook"));
		C.Check(&g_Config.m_CcRainbowWeapon, Localize("Rainbow weapon"));
		C.Check(&g_Config.m_CcRainbowOthers, Localize("Also for other tees"));
		if(g_Config.m_CcRainbowTee || g_Config.m_CcRainbowHook || g_Config.m_CcRainbowWeapon)
		{
			static CButtonContainer s_aModeButtons[4];
			int Mode = g_Config.m_CcRainbowMode - 1;
			const char *apModes[] = {Localize("Rainbow"), Localize("Pulse"), Localize("Dark"), Localize("Random")};
			C.Choice(&Mode, Localize("Rainbow mode"), apModes, 4, s_aModeButtons);
			g_Config.m_CcRainbowMode = Mode + 1;
			C.Slider(&g_Config.m_CcRainbowSpeed, Localize("Rainbow speed"), 10, 500, "%");
		}
		C.Gap(4.0f);
		C.Caption(Localize("Trail"));
		C.Check(&g_Config.m_CcTeeTrail, Localize("Trail behind tees"));
		if(g_Config.m_CcTeeTrail)
		{
			C.Check(&g_Config.m_CcTeeTrailOthers, Localize("Trails of other tees"));
			C.Check(&g_Config.m_CcTeeTrailFade, Localize("Trail fades out"));
			C.Check(&g_Config.m_CcTeeTrailTaper, Localize("Trail gets thinner"));
			static CButtonContainer s_aTrailModeButtons[4];
			int Mode = std::clamp(g_Config.m_CcTeeTrailColorMode - 1, 0, 3);
			const char *apModes[] = {Localize("Color"), Localize("Tee"), Localize("Rainbow"), Localize("Speed")};
			C.Choice(&Mode, Localize("Trail color"), apModes, 4, s_aTrailModeButtons);
			g_Config.m_CcTeeTrailColorMode = Mode + 1;
			if(g_Config.m_CcTeeTrailColorMode == 1)
				C.Color(Localize("Trail color"), &g_Config.m_CcTeeTrailColor, 255);
			C.Slider(&g_Config.m_CcTeeTrailWidth, Localize("Trail width"), 0, 20);
			C.Slider(&g_Config.m_CcTeeTrailLength, Localize("Trail length"), 5, 200);
			C.Slider(&g_Config.m_CcTeeTrailAlpha, Localize("Trail opacity"), 1, 100, "%");
		}
		C.EndCard();
	}

	// ---- tile outlines ----
	{
		CCcColumn &C = Col(CARD_OUTLINES);
		C.BeginCard(CARD_OUTLINES, Localize("Tile outlines"));
		C.Check(&g_Config.m_CcOutline, Localize("Outline special tiles"));
		if(g_Config.m_CcOutline)
		{
			C.Slider(&g_Config.m_CcOutlineWidth, Localize("Outline width"), 1, 10);
			C.Check(&g_Config.m_CcOutlineFreeze, Localize("Freeze"));
			if(g_Config.m_CcOutlineFreeze)
				C.Color(Localize("Freeze color"), &g_Config.m_CcOutlineColorFreeze, 0xFF80FFAA);
			C.Check(&g_Config.m_CcOutlineDeath, Localize("Death tiles"));
			if(g_Config.m_CcOutlineDeath)
				C.Color(Localize("Death tile color"), &g_Config.m_CcOutlineColorDeath, 0xFFE6FFAA);
			C.Check(&g_Config.m_CcOutlineUnfreeze, Localize("Unfreeze"));
			if(g_Config.m_CcOutlineUnfreeze)
				C.Color(Localize("Unfreeze color"), &g_Config.m_CcOutlineColorUnfreeze, 0xFF55FFAA);
			C.Check(&g_Config.m_CcOutlineSolid, Localize("Walls"));
			if(g_Config.m_CcOutlineSolid)
				C.Color(Localize("Wall color"), &g_Config.m_CcOutlineColorSolid, 0xFFBFC8AA);
			C.Check(&g_Config.m_CcOutlineTele, Localize("Teleporters"));
			if(g_Config.m_CcOutlineTele)
				C.Color(Localize("Teleporter color"), &g_Config.m_CcOutlineColorTele, 0xFFBFFFAA);
			C.Check(&g_Config.m_CcOutlineEntities, Localize("Only in entities view"));
			C.Check(&g_Config.m_CcOutlineOuterOnly, Localize("Freeze line only on the open side, not along walls"));
		}
		C.EndCard();
	}

	// ---- music island ----
	{
		CCcColumn &C = Col(CARD_ISLAND);
		C.BeginCard(CARD_ISLAND, Localize("Music island"));
		C.Check(&g_Config.m_CcMusicIsland, Localize("Show the music that is playing (Windows)"), Localize("Shows what plays in YouTube, Spotify and other players, like the iPhone Dynamic Island"));
		if(g_Config.m_CcMusicIsland)
		{
			C.Check(&g_Config.m_CcMusicIslandText, Localize("Show the song title"));
			C.Check(&g_Config.m_CcMusicIslandCover, Localize("Show cover picture"));
			C.Slider(&g_Config.m_CcMusicIslandSize, Localize("Island size"), 50, 200, "%");
			C.Slider(&g_Config.m_CcMusicIslandY, Localize("Distance from the top"), 0, 250);
		}
		C.EndCard();
	}

	// ---- optimization ----
	{
		CCcColumn &C = Col(CARD_PERF);
		C.BeginCard(CARD_PERF, Localize("Optimization"));
		{
			// smooth edges (anti-aliasing): off, 2x, 4x, 8x; works after a restart
			static const int s_aSamples[] = {0, 2, 4, 8};
			static int s_Choice = 0;
			int &Choice = s_Choice;
			Choice = 0;
			for(int i = 0; i < 4; i++)
				if(g_Config.m_GfxFsaaSamples >= s_aSamples[i])
					Choice = i;
			const int Old = Choice;
			static CButtonContainer s_aSmooth[4];
			const char *apSmooth[] = {Localize("Off"), "2x", "4x", "8x"};
			C.Choice(&Choice, Localize("Smooth edges (after restart)"), apSmooth, 4, s_aSmooth);
			if(Choice != Old)
				g_Config.m_GfxFsaaSamples = s_aSamples[Choice];
		}
		C.Check(&g_Config.m_CcUltraSmooth, Localize("Maximum smoothness"), Localize("No FPS limit and no vsync, the game draws as many frames as your PC can. Turn it off and your old settings come back."));
		C.Check(&g_Config.m_CcOptimize, Localize("Optimize the game for my PC"), Localize("Turns off heavy effects; a weaker PC also hides the map decoration. Turn it off and your old settings come back."));
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("FPS right now"), GameClient()->m_CClient.Fps());
		C.Note(aBuf, ColorRGBA(0.55f, 0.9f, 0.65f, 1.0f));
		if(g_Config.m_CcOptimize)
			C.Note(GameClient()->m_CClient.OptimizeStrong() ? Localize("Strong mode: your PC was under 144 FPS") : Localize("Light mode: your PC is fast"));
		C.EndCard();
	}

	// ---- team ----
	{
		CCcColumn &C = Col(CARD_TEAM);
		C.BeginCard(CARD_TEAM, Localize("Team"));
		C.Check(&g_Config.m_CcLastNotify, Localize("Warn when you are the last one alive"));
		C.Check(&g_Config.m_ClShowFreezeBars, Localize("Show when frozen tees unfreeze"));
		if(g_Config.m_ClShowFreezeBars)
		{
			C.Check(&g_Config.m_CcFreezeLine, Localize("Thin line instead of the bar"));
			if(g_Config.m_CcFreezeLine)
			{
				C.Check(&g_Config.m_CcFreezeLineText, Localize("Seconds under the line"));
				C.Check(&g_Config.m_CcFreezeLineInside, Localize("Also while still in freeze"));
				C.Color(Localize("Line color"), &g_Config.m_CcFreezeLineColor, 0xFF9BFF8C);
			}
		}
		C.Check(&g_Config.m_CcPlayerIndicator, Localize("Show teammates that are off screen"));
		if(g_Config.m_CcPlayerIndicator)
		{
			static CButtonContainer s_aIndicatorStyle[2];
			const char *apStyles[] = {Localize("At the screen edge"), Localize("Around your tee")};
			C.Choice(&g_Config.m_CcIndicatorStyle, Localize("Indicator"), apStyles, 2, s_aIndicatorStyle);
			C.Check(&g_Config.m_CcIndicatorFreezeOnly, Localize("Only frozen teammates"));
			if(g_Config.m_CcIndicatorStyle == 0)
			{
				C.Slider(&g_Config.m_CcIndicatorEdgeSize, Localize("Indicator size"), 12, 128);
				C.Check(&g_Config.m_CcIndicatorProximity, Localize("Smaller when further away"));
				C.Check(&g_Config.m_CcIndicatorNinja, Localize("Frozen ones in the ninja skin"));
			}
			else
			{
				C.Check(&g_Config.m_CcIndicatorTees, Localize("Small tees instead of dots"));
				C.Slider(&g_Config.m_CcIndicatorOffset, Localize("Indicator distance"), 50, 400);
				C.Slider(&g_Config.m_CcIndicatorSize, Localize("Indicator size"), 4, 30);
				C.Color(Localize("Not frozen"), &g_Config.m_CcIndicatorColorAlive, 0xFF66FF99);
				C.Color(Localize("Frozen"), &g_Config.m_CcIndicatorColorFrozen, 0xFF99FFB3);
			}
		}
		C.EndCard();
	}

	// ---- HUD ----
	{
		CCcColumn &C = Col(CARD_HUD);
		C.BeginCard(CARD_HUD, Localize("HUD"));
		C.Check(&g_Config.m_CcStatusbar, Localize("Status bar (FPS, ping, team, time)"));
		C.Check(&g_Config.m_ClShowfps, Localize("Show FPS"));
		C.Check(&g_Config.m_ClShowpred, Localize("Show prediction time"));
		C.Check(&g_Config.m_ClShowLocalTimeAlways, Localize("Always show local time"));
		C.Check(&g_Config.m_ClShowhudPlayerPosition, Localize("Show your position"));
		C.Check(&g_Config.m_ClShowhudPlayerSpeed, Localize("Show your speed"));
		C.Check(&g_Config.m_ClShowhudPlayerAngle, Localize("Show your aim angle"));
		C.Check(&g_Config.m_ClShowhudJumpsIndicator, Localize("Show jumps you have left"));
		C.Check(&g_Config.m_ClShowhudDummyActions, Localize("Show dummy actions"));
		C.Check(&g_Config.m_CcMiniVoteHud, Localize("Small vote window"));
		C.Check(&g_Config.m_CcMiniDebug, Localize("Show position and aim angle"));
		C.Check(&g_Config.m_CcRenderCursorSpec, Localize("Cursor while spectating"));
		if(g_Config.m_CcRenderCursorSpec)
			C.Slider(&g_Config.m_CcRenderCursorSpecAlpha, Localize("Cursor opacity"), 0, 100, "%");
		C.Slider(&g_Config.m_ClOverlayEntities, Localize("Game tiles over the map"), 0, 100, "%");
		C.Check(&g_Config.m_CcShowCheckpoint, Localize("Show your checkpoint"));
		C.Check(&g_Config.m_CcTeeCoords, Localize("X tile above every tee"));
		C.Check(&g_Config.m_CcPlayerBelow, Localize("Hint when a tee is straight below you"));
		C.Check(&g_Config.m_CcMaxFps, Localize("FPS counter shows the best frame"));
		C.Check(&g_Config.m_CcVotePercent, Localize("Vote in percent"));
		C.Check(&g_Config.m_CcScoreboardFriend, Localize("Heart after friends in the scoreboard"));
		C.Check(&g_Config.m_CcSpectatorNames, Localize("Names of the spectators"));
		C.Slider(&g_Config.m_CcScoreboardScale, Localize("Scoreboard size"), 50, 100, "%");
		C.Slider(&g_Config.m_CcWheelScale, Localize("Emote wheel size"), 50, 100, "%");
		C.Slider(&g_Config.m_CcUiScale, Localize("Interface size"), 75, 110, "%");
		C.EndCard();
	}

	// ---- gameplay ----
	{
		CCcColumn &C = Col(CARD_GAMEPLAY);
		C.BeginCard(CARD_GAMEPLAY, Localize("Gameplay"));
		C.Check(&g_Config.m_CcGoresMode, Localize("Gores mode"), Localize("You hold the gun, fire swings the hammer"));
		if(g_Config.m_CcGoresMode)
			C.Check(&g_Config.m_CcGoresModeWeapons, Localize("Pause it with shotgun, grenade or laser"));
		C.Check(&g_Config.m_ClSubTickAiming, Localize("Sub-tick aiming"), Localize("Fire and hook go where you aimed at the moment of the click"));
		C.Check(&g_Config.m_CcInputDebounce, Localize("Input debounce"), Localize("Very short taps of jump, hook and walking are not lost"));
		C.Check(&g_Config.m_CcFinishPrediction, Localize("Finish prediction"), Localize("How far you are to the finish and your expected time"));
		C.Check(&g_Config.m_CcEdgeInfo, Localize("Edge info"), Localize("Tiles to the next freeze left, below and right"));
		C.Check(&g_Config.m_CcFineZoom, Localize("Finer zoom (half steps)"));
		C.Check(&g_Config.m_CcCinematicCamera, Localize("Cinematic spectator camera"));
		if(g_Config.m_CcCinematicCamera)
			C.Slider(&g_Config.m_CcCinematicCameraStrength, Localize("Smoothness"), 0, 100, "%");
		C.Check(&g_Config.m_CcBetterSpectate, Localize("Better spectating"), Localize("Free view: click the tee in the middle to follow it, right click to look freely"));
		C.Check(&g_Config.m_CcSpecMovedNotify, Localize("Warn when your tee is moved while paused"));
		C.Check(&g_Config.m_CcConfirmQuit, Localize("Always ask before quitting"));
		C.Check(&g_Config.m_CcHighPriority, Localize("High process priority"), Localize("Windows gives the game more CPU time"));
		C.Gap(4.0f);
		C.Caption(Localize("Fast actions"));
		{
			char *apActions[] = {g_Config.m_CcFastAction1, g_Config.m_CcFastAction2, g_Config.m_CcFastAction3, g_Config.m_CcFastAction4, g_Config.m_CcFastAction5, g_Config.m_CcFastAction6};
			static CLineInput s_aInputs[6];
			static CButtonContainer s_aReaders[6];
			static CButtonContainer s_aClears[6];
			for(int i = 0; i < 6; i++)
			{
				s_aInputs[i].SetBuffer(apActions[i], sizeof(g_Config.m_CcFastAction1));
				char aLabel[32], aCommand[32];
				str_format(aLabel, sizeof(aLabel), "%s %d", Localize("Action"), i + 1);
				str_format(aCommand, sizeof(aCommand), "cc_fast_action %d", i + 1);
				C.Edit(&s_aInputs[i], aLabel);
				CcKeyRow(Ui(), GameClient(), C.Line(20.0f), Localize("Key"), aCommand, &s_aReaders[i], &s_aClears[i]);
			}
		}
		C.EndCard();
	}

	// ---- configs ----
	{
		CCcColumn &C = Col(CARD_CONFIGS);
		C.BeginCard(CARD_CONFIGS, Localize("Configs"));
		static std::vector<SCcConfigFile> s_vConfigs;
		static bool s_Scanned = false;
		static int s_Selected = -1;
		static char s_aName[64] = "";
		static CLineInput s_NameInput(s_aName, sizeof(s_aName));
		if(!s_Scanned)
		{
			Storage()->CreateFolder("cclient_configs", IStorage::TYPE_SAVE);
			s_vConfigs.clear();
			Storage()->ListDirectoryInfo(IStorage::TYPE_SAVE, "cclient_configs", CcConfigScan, &s_vConfigs);
			std::sort(s_vConfigs.begin(), s_vConfigs.end(), [](const SCcConfigFile &a, const SCcConfigFile &b) { return a.m_Time > b.m_Time; });
			s_Scanned = true;
			if(s_Selected >= (int)s_vConfigs.size())
				s_Selected = -1;
		}
		// the list: name and date, click to select
		static std::vector<CButtonContainer> s_vRowButtons;
		s_vRowButtons.resize(s_vConfigs.size());
		for(size_t i = 0; i < s_vConfigs.size(); i++)
		{
			CUIRect Row = C.Line(20.0f), Date;
			const bool Selected = (int)i == s_Selected;
			const float Hover = Ui()->HoverAnim(&s_vRowButtons[i]);
			Row.Draw(Selected ? CCStyle::ACCENT.WithAlpha(0.45f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.04f + 0.05f * Hover), IGraphics::CORNER_ALL, 6.0f);
			if(Ui()->DoButtonLogic(&s_vRowButtons[i], 0, &Row, BUTTONFLAG_LEFT))
			{
				s_Selected = i;
				str_copy(s_aName, s_vConfigs[i].m_Name.c_str());
			}
			Row.VMargin(8.0f, &Row);
			Row.VSplitRight(90.0f, &Row, &Date);
			Ui()->DoLabel(&Row, s_vConfigs[i].m_Name.c_str(), 12.0f, TEXTALIGN_ML);
			char aDate[32];
			str_timestamp_ex(s_vConfigs[i].m_Time, aDate, sizeof(aDate), "%d.%m.%Y %H:%M");
			TextRender()->TextColor(CCStyle::TEXT_DIM);
			Ui()->DoLabel(&Date, aDate, 10.0f, TEXTALIGN_MR);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		if(s_vConfigs.empty())
			C.Note(Localize("No saved configs yet"));
		C.Edit(&s_NameInput, Localize("Name"));
		char aFile[IO_MAX_PATH_LENGTH];
		str_format(aFile, sizeof(aFile), "cclient_configs/%s.cfg", s_aName);
		const bool HasName = s_aName[0] && !str_find(s_aName, "/") && !str_find(s_aName, "\\") && !str_find(s_aName, "..");
		static CButtonContainer s_SaveButton, s_LoadButton, s_RenameButton, s_DeleteButton, s_FolderButton;
		if(C.Button(&s_SaveButton, Localize("Save current settings under this name")) && HasName)
		{
			ConfigManager()->Save();
			if(CcCopyFile(Storage(), CONFIG_FILE, aFile))
				s_Scanned = false;
		}
		if(s_Selected >= 0 && s_Selected < (int)s_vConfigs.size())
		{
			char aSelected[IO_MAX_PATH_LENGTH];
			str_format(aSelected, sizeof(aSelected), "cclient_configs/%s.cfg", s_vConfigs[s_Selected].m_Name.c_str());
			if(C.Button(&s_LoadButton, Localize("Load the selected config")))
			{
				Console()->ExecuteFile(aSelected, IConsole::CLIENT_ID_UNSPECIFIED);
				ConfigManager()->Save();
			}
			if(C.Button(&s_RenameButton, Localize("Rename the selected config to the name")) && HasName && str_comp(aSelected, aFile) != 0)
			{
				Storage()->RenameFile(aSelected, aFile, IStorage::TYPE_SAVE);
				s_Scanned = false;
			}
			if(C.Button(&s_DeleteButton, Localize("Delete the selected config")))
			{
				Storage()->RemoveFile(aSelected, IStorage::TYPE_SAVE);
				s_Selected = -1;
				s_Scanned = false;
			}
		}
		if(C.Button(&s_FolderButton, Localize("Open the configs folder")))
		{
			char aPath[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "cclient_configs", aPath, sizeof(aPath));
			Client()->ViewFile(aPath);
			s_Scanned = false;
		}
		C.EndCard();
	}

	// ---- effects ----
	{
		CCcColumn &C = Col(CARD_EFFECTS);
		C.BeginCard(CARD_EFFECTS, Localize("Effects"));
		C.Check(&g_Config.m_CcJelly, Localize("Jelly tees"));
		if(g_Config.m_CcJelly)
		{
			C.Check(&g_Config.m_CcJellyOthers, Localize("Also other tees"));
			C.Slider(&g_Config.m_CcJellyStrength, Localize("Wobble"), 1, 100, "%");
		}
		C.Gap(4.0f);
		C.Caption(Localize("Weapon glow"));
		C.Check(&g_Config.m_CcGlowLaser, Localize("Laser"));
		C.Check(&g_Config.m_CcGlowShotgun, Localize("Shotgun"));
		C.Check(&g_Config.m_CcGlowRocket, Localize("Grenade"));
		if(g_Config.m_CcGlowLaser || g_Config.m_CcGlowShotgun || g_Config.m_CcGlowRocket)
		{
			static CButtonContainer s_aGlowModes[4];
			const char *apModes[] = {Localize("Classic"), Localize("Prism"), Localize("Pulse"), Localize("Crystal")};
			C.Choice(&g_Config.m_CcGlowMode, Localize("Glow look"), apModes, 4, s_aGlowModes);
			C.Slider(&g_Config.m_CcGlowPower, Localize("Glow strength"), 0, 100, "%");
			if(g_Config.m_CcGlowMode == 0 || g_Config.m_CcGlowMode == 2)
				C.Color(Localize("Glow color"), &g_Config.m_CcGlowColor, 0xF8FF80);
		}
		C.Gap(4.0f);
		C.Caption(Localize("Hook and hammer"));
		C.Check(&g_Config.m_CcAirJumpRing, Localize("Ring when jumping in the air"));
		C.Check(&g_Config.m_CcHammerSlash, Localize("Hammer swing"));
		C.Check(&g_Config.m_CcNoHammerParticles, Localize("No hammer hit flash"));
		C.Check(&g_Config.m_CcHookLasso, Localize("Lasso on the hooked tee"));
		C.Check(&g_Config.m_CcHookBox, Localize("Box on the hooked tee"));
		C.Check(&g_Config.m_CcHookHold, Localize("Light along the hook while holding"));
		static CLineInput s_HookInput(g_Config.m_CcCustomHook, sizeof(g_Config.m_CcCustomHook));
		C.Edit(&s_HookInput, Localize("Hook from game pack (name in assets/game)"));
		if(g_Config.m_CcCustomHook[0])
			C.Check(&g_Config.m_CcCustomHookOthers, Localize("Others get this hook too"));
		C.Gap(4.0f);
		C.Caption(Localize("Freeze"));
		C.Check(&g_Config.m_CcFreezeFrost, Localize("Ice around frozen tees"));
		C.Check(&g_Config.m_CcFreezeLightning, Localize("Lightning when a tee gets frozen"));
		if(g_Config.m_CcFreezeLightning)
			C.Color(Localize("Lightning color"), &g_Config.m_CcFreezeLightningColor, 0x8FFF9F);
		C.Gap(4.0f);
		static CButtonContainer s_aWeather[5];
		const char *apWeather[] = {Localize("Off"), Localize("Snow"), Localize("Rain"), Localize("Fireflies"), Localize("Dust")};
		C.Choice(&g_Config.m_CcWeather, Localize("Weather"), apWeather, 5, s_aWeather);
		if(g_Config.m_CcWeather)
			C.Slider(&g_Config.m_CcWeatherAmount, Localize("Amount"), 25, 300, "%");
		C.Gap(4.0f);
		C.Caption(Localize("More"));
		C.Check(&g_Config.m_CcMotionBlur, Localize("Motion blur"));
		if(g_Config.m_CcMotionBlur)
			C.Slider(&g_Config.m_CcMotionBlurStrength, Localize("Blur strength"), 1, 100, "%");
		C.Check(&g_Config.m_CcAnimChat, Localize("Chat messages slide in"));
		C.Check(&g_Config.m_CcAnimKillfeed, Localize("Kill messages slide in"));
		C.Check(&g_Config.m_CcNameGradient, Localize("Names in a gradient"));
		if(g_Config.m_CcNameGradient)
		{
			C.Color(Localize("First color"), &g_Config.m_CcNameGradientA, 0xF8FF9A);
			C.Color(Localize("Second color"), &g_Config.m_CcNameGradientB, 0xFFFFFF);
			C.Check(&g_Config.m_CcNameGradientMove, Localize("Moving gradient"));
		}
		C.Check(&g_Config.m_CcFlyingNamePlates, Localize("Flying name plates"));
		if(g_Config.m_CcFlyingNamePlates)
			C.Slider(&g_Config.m_CcFlyingNamePlatesSpeed, Localize("How fast they follow"), 1, 100, "%");
		C.Check(&g_Config.m_CcChatBubbles, Localize("Chat bubbles above tees"));
		if(g_Config.m_CcChatBubbles)
			C.Slider(&g_Config.m_CcChatBubbleTime, Localize("Bubble time"), 2, 15, "s");
		C.Check(&g_Config.m_CcCursorTrail, Localize("Cursor trail"));
		if(g_Config.m_CcCursorTrail)
			C.Slider(&g_Config.m_CcCursorTrailLength, Localize("Trail length"), 4, 60);
		C.Check(&g_Config.m_CcKeystrokes, Localize("Your keys on the screen"));
		if(g_Config.m_CcKeystrokes)
		{
			C.Slider(&g_Config.m_CcKeystrokesX, Localize("Keys position X"), 0, 100, "%");
			C.Slider(&g_Config.m_CcKeystrokesY, Localize("Keys position Y"), 0, 100, "%");
		}
		C.Check(&g_Config.m_Cc3dParticles, Localize("3D particles"));
		if(g_Config.m_Cc3dParticles)
			C.Slider(&g_Config.m_Cc3dParticlesCount, Localize("Number of particles"), 5, 100);
		{
			static CButtonContainer s_aAspect[4];
			const char *apAspect[] = {Localize("Native"), "4:3", "5:4", "16:10"};
			C.Choice(&g_Config.m_CcAspectRatio, Localize("Stretched resolution"), apAspect, 4, s_aAspect);
		}
		C.Check(&g_Config.m_CcPhysBalls, Localize("Physics balls"));
		if(g_Config.m_CcPhysBalls)
			C.Slider(&g_Config.m_CcPhysBallsCount, Localize("Number of balls"), 1, 30);
		C.EndCard();
	}

	// ---- save wheel and markers ----
	{
		CCcColumn &C = Col(CARD_SAVES);
		C.BeginCard(CARD_SAVES, Localize("Saves and markers"));
		C.Note(Localize("Hold Shift and press Q or E: save wheel"), CCStyle::TEXT);
		C.Note(Localize("Middle mouse button: marker for your team"), CCStyle::TEXT);
		C.Gap(4.0f);
		C.Caption(Localize("Keys"));
		{
			// any key for the wheel, the marker and each save picture
			struct SMarkerBind
			{
				const char *m_pLabel;
				const char *m_pCommand;
			};
			const SMarkerBind aBinds[] = {
				{Localize("Save wheel (hold)"), "+cc_save_wheel"},
				{Localize("Marker where you aim"), "cc_ping"},
				{Localize("Hook me up"), "cc_save 0"},
				{Localize("Hook me to the right"), "cc_save 1"},
				{Localize("Hammer me"), "cc_save 2"},
				{Localize("Stand under me"), "cc_save 3"},
				{Localize("Pull me down"), "cc_save 4"},
				{Localize("Hook me to the left"), "cc_save 5"},
				{Localize("Wait, don't touch me"), "cc_save 6"},
				{Localize("Thanks!"), "cc_save 7"},
			};
			static CButtonContainer s_aReaders[std::size(aBinds)];
			static CButtonContainer s_aClears[std::size(aBinds)];
			for(size_t i = 0; i < std::size(aBinds); i++)
				CcKeyRow(Ui(), GameClient(), C.Line(20.0f), aBinds[i].m_pLabel, aBinds[i].m_pCommand, &s_aReaders[i], &s_aClears[i]);
		}
		C.EndCard();
	}

	// ---- chat ----
	{
		CCcColumn &C = Col(CARD_CHAT);
		C.BeginCard(CARD_CHAT, Localize("Chat"));
		C.Check(&g_Config.m_ClShowKillMessages, Localize("Show kill messages"));
		C.Check(&g_Config.m_ClShowChatFriends, Localize("Only show chat of friends"));
		C.Check(&g_Config.m_ClChatTeamColors, Localize("Names in team colors"));
		C.Check(&g_Config.m_CcChatInteract, Localize("Click messages while typing"), Localize("Reply, mention, copy the message or the name, mute"));
		C.Check(&g_Config.m_CcSilentTyping, Localize("Silent typing"), Localize("Others don't see that you are writing"));
		C.Check(&g_Config.m_CcChatLayoutFix, Localize("Fix commands typed in the Russian layout"), Localize(".ыфму becomes /save"));
		C.EndCard();
	}

	// ---- updates ----
	{
		CCcColumn &C = Col(CARD_UPDATES);
		C.BeginCard(CARD_UPDATES, Localize("Updates"));
		CCClient &CClient = GameClient()->m_CClient;
		char aBuf[128];
		if(CCClient::CurrentBuild() > 0)
			str_format(aBuf, sizeof(aBuf), "%s: #%d", Localize("Your version"), CCClient::CurrentBuild());
		else
			str_copy(aBuf, Localize("Your version: local build (no updates)"));
		C.Note(aBuf);
		C.Check(&g_Config.m_CcUpdateCheck, Localize("Check for updates when the game starts"));
		if(CClient.CanSelfUpdate())
		{
			static CButtonContainer s_CheckButton;
			const CCClient::EUpdateState State = CClient.UpdateState();
			const char *pLabel = Localize("Check for updates");
			if(State == CCClient::EUpdateState::CHECKING)
				pLabel = Localize("Checking…");
			else if(State == CCClient::EUpdateState::UP_TO_DATE)
				pLabel = Localize("You have the newest version");
			else if(State == CCClient::EUpdateState::AVAILABLE)
				pLabel = Localize("Update is ready: see the main menu");
			if(C.Button(&s_CheckButton, pLabel))
				CClient.CheckForUpdate();
		}
		C.EndCard();
	}

	// ---- automation that the game already supports ----
	{
		CCcColumn &C = Col(CARD_ON_JOIN);
		C.BeginCard(CARD_ON_JOIN, Localize("On join"));
		static CLineInput s_RunOnJoinInput(g_Config.m_ClRunOnJoin, sizeof(g_Config.m_ClRunOnJoin));
		C.Edit(&s_RunOnJoinInput, Localize("Console command when you join a server"));
		static CLineInput s_OnConnectInput(g_Config.m_CcExecuteOnConnect, sizeof(g_Config.m_CcExecuteOnConnect));
		C.Edit(&s_OnConnectInput, Localize("Command before connecting"));
		static CLineInput s_OnJoinLateInput(g_Config.m_CcExecuteOnJoin, sizeof(g_Config.m_CcExecuteOnJoin));
		C.Edit(&s_OnJoinLateInput, Localize("Command a bit after joining"));
		C.Slider(&g_Config.m_CcExecuteOnJoinDelay, Localize("Delay (ticks)"), 7, 500);
		C.EndCard();
	}

	// ---- automation ----
	{
		CCcColumn &C = Col(CARD_AUTOMATION);
		C.BeginCard(CARD_AUTOMATION, Localize("Automation"));
		C.Check(&g_Config.m_CcAutoVoteWhenFar, Localize("Vote no to map changes when you are far"), Localize("Not when a friend, your team or you yourself started the vote"));
		if(g_Config.m_CcAutoVoteWhenFar)
		{
			C.Slider(&g_Config.m_CcAutoVoteWhenFarTime, Localize("Far means after"), 0, 20, " min");
			static CLineInput s_FarMessage(g_Config.m_CcAutoVoteWhenFarMessage, sizeof(g_Config.m_CcAutoVoteWhenFarMessage));
			C.Edit(&s_FarMessage, Localize("Message in chat (empty: none)"));
		}
		C.Check(&g_Config.m_CcAutoReplyMuted, Localize("Auto reply to muted players"));
		if(g_Config.m_CcAutoReplyMuted)
		{
			static CLineInput s_MutedMessage(g_Config.m_CcAutoReplyMutedMessage, sizeof(g_Config.m_CcAutoReplyMutedMessage));
			C.Edit(&s_MutedMessage, Localize("Reply"));
		}
		C.Check(&g_Config.m_CcAutoReplyMinimized, Localize("Auto reply when the game is minimized"));
		if(g_Config.m_CcAutoReplyMinimized)
		{
			static CLineInput s_MinimizedMessage(g_Config.m_CcAutoReplyMinimizedMessage, sizeof(g_Config.m_CcAutoReplyMinimizedMessage));
			C.Edit(&s_MinimizedMessage, Localize("Reply"));
		}
		C.Check(&g_Config.m_CcChangeNameNearFinish, Localize("Other name near the finish"));
		if(g_Config.m_CcChangeNameNearFinish)
		{
			static CLineInput s_FinishName(g_Config.m_CcFinishName, sizeof(g_Config.m_CcFinishName));
			C.Edit(&s_FinishName, Localize("Name near the finish"));
		}
		C.EndCard();
	}

	// ---- ghosts ----
	{
		CCcColumn &C = Col(CARD_GHOSTS);
		C.BeginCard(CARD_GHOSTS, Localize("Ghosts"));
		C.Check(&g_Config.m_CcShowOthersGhosts, Localize("Ghosts of other tees (without prediction)"));
		if(g_Config.m_CcShowOthersGhosts)
		{
			C.Check(&g_Config.m_CcSwapGhosts, Localize("Swap ghosts and tees"));
			C.Check(&g_Config.m_CcHideFrozenGhosts, Localize("No ghosts of frozen tees"));
			C.Check(&g_Config.m_CcRenderGhostAsCircle, Localize("Ghosts as circles"));
			C.Slider(&g_Config.m_CcPredGhostsAlpha, Localize("Predicted tees"), 0, 100, "%");
			C.Slider(&g_Config.m_CcUnpredGhostsAlpha, Localize("Ghosts"), 0, 100, "%");
		}
		C.EndCard();
	}

	// ---- just for fun ----
	{
		CCcColumn &C = Col(CARD_FUN);
		C.BeginCard(CARD_FUN, Localize("Just for fun"));
		C.Check(&g_Config.m_CcPetShow, Localize("Pet next to your tee"));
		if(g_Config.m_CcPetShow)
		{
			static CLineInput s_PetSkin(g_Config.m_CcPetSkin, sizeof(g_Config.m_CcPetSkin));
			C.Edit(&s_PetSkin, Localize("Skin of the pet"));
			C.Slider(&g_Config.m_CcPetSize, Localize("Pet size"), 10, 500, "%");
			C.Slider(&g_Config.m_CcPetAlpha, Localize("Pet opacity"), 10, 100, "%");
		}
		C.Gap(4.0f);
		C.Caption(Localize("Drawing on the map"));
		static CButtonContainer s_BindDrawButton;
		if(C.Button(&s_BindDrawButton, Localize("Draw with mouse button 4, erase with 5")))
			Console()->ExecuteLine("bind mouse4 +bg_draw; bind mouse5 +bg_draw_erase", IConsole::CLIENT_ID_UNSPECIFIED);
		static CButtonContainer s_ClearDrawButton;
		if(C.Button(&s_ClearDrawButton, Localize("Clear all drawings")))
			Console()->ExecuteLine("bg_draw_reset", IConsole::CLIENT_ID_UNSPECIFIED);
		C.Color(Localize("Drawing color"), &g_Config.m_CcBgDrawColor, 14024576);
		C.Slider(&g_Config.m_CcBgDrawWidth, Localize("Line width"), 1, 50);
		C.Slider(&g_Config.m_CcBgDrawFadeTime, Localize("Lines disappear after"), 0, 600, "s");
		C.Slider(&g_Config.m_CcBgDrawMaxItems, Localize("Most lines"), 0, 2048);
		C.Check(&g_Config.m_CcBgDrawAutoSaveLoad, Localize("Keep drawings for every map"));
		C.EndCard();
	}

	// let the scroll region know how tall the content is
	float Used = 0.0f;
	for(const CCcColumn &Column : vColumns)
		Used = std::max(Used, Column.Used());
	CUIRect Content = MainView;
	Content.h = Used;
	s_ScrollRegion.AddRect(Content);
	s_ScrollRegion.End();
}
