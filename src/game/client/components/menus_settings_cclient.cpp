/* Cr1stal Client (C-Client) settings page. Based on DDNet, see licence.txt. */
#include "menus.h"

#include <base/str.h>

#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/components/cclient.h>
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

	void Color(const char *pText, unsigned *pValue, unsigned DefaultValue)
	{
		static CButtonContainer s_aResetButtons[16];
		static int s_NextReset = 0;
		CButtonContainer *pReset = &s_aResetButtons[s_NextReset++ % 16];
		m_pMenus->DoLine_ColorPicker(pReset, CC_LINE, 12.0f, 3.0f, &m_Rect, pText, pValue, color_cast<ColorRGBA>(ColorHSLA(DefaultValue, true)), false, nullptr, true);
	}

	// row of buttons, one of them selected
	void Choice(int *pValue, const char *pLabel, const char *const *ppOptions, int NumOptions, CButtonContainer *pButtons)
	{
		CUIRect Row = Line();
		CUIRect Label;
		Row.VSplitLeft(110.0f, &Label, &Row);
		m_pUi->DoLabel(&Label, pLabel, 12.0f, TEXTALIGN_ML);
		const float ButtonW = Row.w / NumOptions;
		for(int i = 0; i < NumOptions; i++)
		{
			CUIRect Button;
			Row.VSplitLeft(ButtonW, &Button, &Row);
			Button.VMargin(1.0f, &Button);
			const int Corners = i == 0 ? IGraphics::CORNER_L : (i == NumOptions - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
			const ColorRGBA Color = *pValue == i ? ColorRGBA(0.45f, 0.25f, 0.75f, 0.9f) : ColorRGBA(0.15f, 0.08f, 0.25f, 0.8f);
			if(m_pMenus->DoButton_Menu(&pButtons[i], ppOptions[i], 0, &Button, BUTTONFLAG_LEFT, nullptr, Corners, 5.0f, 0.0f, Color))
				*pValue = i;
		}
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

	// ---- input modes ----
	Left.Headline(Localize("Input"));
	{
		static CButtonContainer s_aInputButtons[4];
		const char *apModes[] = {Localize("Off"), "Fast", "Sheylun", "Delta"};
		Left.Choice(&g_Config.m_CcFastInput, Localize("Input mode"), apModes, 4, s_aInputButtons);
		const ColorRGBA NoteColor(0.75f, 0.85f, 1.0f, 1.0f);
		switch(g_Config.m_CcFastInput)
		{
		case 1:
			Left.Note(Localize("Fast Input: your tee reacts before the server tick"), NoteColor);
			Left.Slider(&g_Config.m_CcFastInputAmount, Localize("Amount"), 20, 100, "ms");
			break;
		case 2:
			Left.Note(Localize("Sheylun Input: predicts whole ticks, as many as your ping needs"), NoteColor);
			Left.Slider(&g_Config.m_CcSheylunStrength, Localize("Strength"), 25, 200, "%");
			Left.Slider(&g_Config.m_CcSheylunMaxTicks, Localize("Max ticks"), 1, 6);
			Left.Check(&g_Config.m_CcSheylunTickAim, Localize("Aim updates once per tick (calmer hook line)"));
			{
				char aBuf[64];
				str_format(aBuf, sizeof(aBuf), "%s: %d", Localize("Ticks right now"), GameClient()->m_Controls.FastInputTicks());
				Left.Note(aBuf, ColorRGBA(0.6f, 0.95f, 0.7f, 1.0f));
			}
			break;
		case 3:
			Left.Note(Localize("Delta Input: Fast Input plus finer aim for small mouse moves"), NoteColor);
			Left.Slider(&g_Config.m_CcFastInputAmount, Localize("Amount"), 20, 100, "ms");
			Left.Slider(&g_Config.m_CcDeltaPrecision, Localize("Small move sensitivity"), 20, 200, "%");
			Left.Slider(&g_Config.m_CcDeltaThreshold, Localize("Small move size"), 1, 30);
			break;
		default:
			Left.Note(Localize("Normal DDNet input"), NoteColor);
		}
		if(g_Config.m_CcFastInput == 1 || g_Config.m_CcFastInput == 2)
			Left.Note(Localize("Only changes what you see, not what the server gets"), ColorRGBA(0.6f, 0.6f, 0.75f, 1.0f));
	}
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

	Left.Gap();

	// ---- customization ----
	Left.Headline(Localize("Customization"));
	{
		// font for all text in the game
		CUIRect Row = Left.Line();
		CUIRect Label, Prev, Next, Name;
		Row.VSplitLeft(110.0f, &Label, &Row);
		Ui()->DoLabel(&Label, Localize("Font"), 12.0f, TEXTALIGN_ML);
		Row.VSplitLeft(24.0f, &Prev, &Row);
		Row.VSplitRight(24.0f, &Row, &Next);
		Name = Row;
		static CButtonContainer s_PrevFont, s_NextFont;
		if(DoButton_Menu(&s_PrevFont, "<", 0, &Prev, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
			g_Config.m_CcFont = (g_Config.m_CcFont + CCClient::NUM_FONTS - 1) % CCClient::NUM_FONTS;
		if(DoButton_Menu(&s_NextFont, ">", 0, &Next, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
			g_Config.m_CcFont = (g_Config.m_CcFont + 1) % CCClient::NUM_FONTS;
		Name.Draw(ColorRGBA(0.15f, 0.08f, 0.25f, 0.8f), IGraphics::CORNER_NONE, 0.0f);
		Ui()->DoLabel(&Name, CCClient::FontDisplayName(g_Config.m_CcFont), 12.0f, TEXTALIGN_MC);
		Left.Note(Localize("The font changes all text in the game right away"), ColorRGBA(0.75f, 0.75f, 0.9f, 1.0f));
	}
	{
		static CButtonContainer s_aHammerButtons[3];
		const char *apHammer[] = {Localize("Normal"), Localize("Follows cursor"), Localize("Like a gun")};
		Left.Choice(&g_Config.m_CcHammerMode, Localize("Hammer"), apHammer, 3, s_aHammerButtons);
	}
	{
		static CButtonContainer s_aAuraButtons[CCClient::NUM_AURAS + 1];
		const char *apAura[] = {Localize("Off"), "1", "2", "3", "4"};
		Left.Choice(&g_Config.m_CcAura, Localize("Picture behind tee"), apAura, CCClient::NUM_AURAS + 1, s_aAuraButtons);
		if(g_Config.m_CcAura)
		{
			Left.Slider(&g_Config.m_CcAuraSize, Localize("Picture size"), 80, 600);
			Left.Slider(&g_Config.m_CcAuraAlpha, Localize("Picture opacity"), 5, 100, "%");
			Left.Slider(&g_Config.m_CcAuraSpin, Localize("Picture rotation"), 0, 300);
		}
	}
	Left.Note(Localize("Texture packs of other players: coming later"), ColorRGBA(0.6f, 0.6f, 0.75f, 1.0f));
	Left.Gap();

	// ---- tile outlines ----
	Left.Headline(Localize("Tile outlines"));
	Left.Check(&g_Config.m_CcOutline, Localize("Outline special tiles"));
	if(g_Config.m_CcOutline)
	{
		Left.Slider(&g_Config.m_CcOutlineWidth, Localize("Outline width"), 1, 10);
		Left.Check(&g_Config.m_CcOutlineFreeze, Localize("Freeze"));
		if(g_Config.m_CcOutlineFreeze)
			Left.Color(Localize("Freeze color"), &g_Config.m_CcOutlineColorFreeze, 0xFF80FFAA);
		Left.Check(&g_Config.m_CcOutlineDeath, Localize("Death tiles"));
		if(g_Config.m_CcOutlineDeath)
			Left.Color(Localize("Death tile color"), &g_Config.m_CcOutlineColorDeath, 0xFFE6FFAA);
		Left.Check(&g_Config.m_CcOutlineUnfreeze, Localize("Unfreeze"));
		if(g_Config.m_CcOutlineUnfreeze)
			Left.Color(Localize("Unfreeze color"), &g_Config.m_CcOutlineColorUnfreeze, 0xFF55FFAA);
		Left.Check(&g_Config.m_CcOutlineSolid, Localize("Walls"));
		if(g_Config.m_CcOutlineSolid)
			Left.Color(Localize("Wall color"), &g_Config.m_CcOutlineColorSolid, 0xFFBFC8AA);
	}
	Left.Gap();

	// ---- music island ----
	Left.Headline(Localize("Music island"));
	Left.Check(&g_Config.m_CcMusicIsland, Localize("Show the music that is playing (Windows)"), Localize("Shows what plays in YouTube, Spotify and other players, like the iPhone Dynamic Island"));
	if(g_Config.m_CcMusicIsland)
	{
		Left.Check(&g_Config.m_CcMusicIslandText, Localize("Show title and artist"));
		Left.Check(&g_Config.m_CcMusicIslandCover, Localize("Show cover picture"));
		Left.Slider(&g_Config.m_CcMusicIslandSize, Localize("Island size"), 50, 200, "%");
		Left.Slider(&g_Config.m_CcMusicIslandY, Localize("Distance from the top"), 0, 250);
	}

	// ================= right column =================

	// ---- skin builder ----
	Right.Headline(Localize("Skin Builder"));
	{
		CCClient &CClient = GameClient()->m_CClient;
		CUIRect Area = Right.Line(170.0f);
		CUIRect PreviewBox, Controls;
		Area.VSplitLeft(130.0f, &PreviewBox, &Controls);
		Controls.VSplitLeft(10.0f, nullptr, &Controls);
		PreviewBox.h = 130.0f;
		PreviewBox.Draw(ColorRGBA(0.06f, 0.02f, 0.12f, 0.8f), IGraphics::CORNER_ALL, 10.0f);

		// preview: the composed skin drawn like a tee (back foot, body, front foot, eyes)
		const IGraphics::CTextureHandle Texture = CClient.SkinBuilderPreview();
		if(Texture.IsValid())
		{
			const vec2 Center = PreviewBox.Center() + vec2(0.0f, -6.0f);
			const float Size = 96.0f;
			Graphics()->TextureSet(Texture);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			auto Part = [&](float U0, float V0, float U1, float V1, vec2 Pos, float PartW, float PartH) {
				Graphics()->QuadsSetSubset(U0, V0, U1, V1);
				IGraphics::CQuadItem Quad(Pos.x, Pos.y, PartW, PartH);
				Graphics()->QuadsDraw(&Quad, 1);
			};
			const vec2 BackFoot = Center + vec2(-Size * 0.11f, Size * 0.3f);
			const vec2 FrontFoot = Center + vec2(Size * 0.11f, Size * 0.33f);
			const vec2 BodyPos = Center + vec2(0.0f, -Size * 0.04f);
			// outlines first
			Part(0.75f, 0.5f, 1.0f, 0.75f, BackFoot, Size * 0.5f, Size * 0.25f);
			Part(0.375f, 0.0f, 0.75f, 0.75f, BodyPos, Size, Size);
			Part(0.75f, 0.5f, 1.0f, 0.75f, FrontFoot, Size * 0.5f, Size * 0.25f);
			Part(0.75f, 0.25f, 1.0f, 0.5f, BackFoot, Size * 0.5f, Size * 0.25f);
			Part(0.0f, 0.0f, 0.375f, 0.75f, BodyPos, Size, Size);
			Part(0.75f, 0.25f, 1.0f, 0.5f, FrontFoot, Size * 0.5f, Size * 0.25f);
			const float EyeSize = Size * 0.4f;
			Part(0.25f, 0.75f, 0.375f, 1.0f, BodyPos + vec2(Size * 0.05f, -Size * 0.09f), EyeSize, EyeSize);
			Part(0.375f, 0.75f, 0.25f, 1.0f, BodyPos + vec2(Size * 0.2f, -Size * 0.09f), EyeSize, EyeSize);
			Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
			Graphics()->QuadsEnd();
		}
		else
		{
			Ui()->DoLabel(&PreviewBox, Localize("Parts not found"), 11.0f, TEXTALIGN_MC);
		}

		// arrows for every part
		static CButtonContainer s_aSbButtons[12];
		int ButtonIndex = 0;
		auto ArrowRow = [&](const char *pLabel, int *pValue, int Count, bool IsColor) {
			CUIRect Row, Label, Prev, Next, Value;
			Controls.HSplitTop(CC_LINE, &Row, &Controls);
			Controls.HSplitTop(4.0f, nullptr, &Controls);
			Row.VSplitLeft(70.0f, &Label, &Row);
			Ui()->DoLabel(&Label, pLabel, 12.0f, TEXTALIGN_ML);
			Row.VSplitLeft(22.0f, &Prev, &Row);
			Row.VSplitRight(22.0f, &Row, &Next);
			Value = Row;
			if(DoButton_Menu(&s_aSbButtons[ButtonIndex++], "<", 0, &Prev, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L, 5.0f))
				*pValue = (*pValue + Count - 1) % Count;
			if(DoButton_Menu(&s_aSbButtons[ButtonIndex++], ">", 0, &Next, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R, 5.0f))
				*pValue = (*pValue + 1) % Count;
			if(IsColor)
			{
				Value.Draw(CCClient::SkinBuilderColor(*pValue), IGraphics::CORNER_NONE, 0.0f);
			}
			else
			{
				Value.Draw(ColorRGBA(0.15f, 0.08f, 0.25f, 0.8f), IGraphics::CORNER_NONE, 0.0f);
				char aBuf[16];
				str_format(aBuf, sizeof(aBuf), "%d / %d", *pValue + 1, Count);
				Ui()->DoLabel(&Value, aBuf, 12.0f, TEXTALIGN_MC);
			}
		};
		ArrowRow(Localize("Body"), &g_Config.m_CcSbBody, CCClient::SB_PARTS, false);
		ArrowRow(Localize("Ears"), &g_Config.m_CcSbEars, CCClient::SB_PARTS, false);
		ArrowRow(Localize("Feet"), &g_Config.m_CcSbFeet, CCClient::SB_PARTS, false);
		ArrowRow(Localize("Face"), &g_Config.m_CcSbEyes, CCClient::SB_PARTS, false);
		ArrowRow(Localize("Body color"), &g_Config.m_CcSbBodyColor, CCClient::SB_COLORS, true);
		ArrowRow(Localize("Feet color"), &g_Config.m_CcSbFeetColor, CCClient::SB_COLORS, true);

		// name + save
		static CLineInput s_SkinNameInput(g_Config.m_CcSbName, sizeof(g_Config.m_CcSbName));
		CUIRect Row = Right.Line();
		CUIRect NameBox, SaveButton;
		Row.VSplitRight(150.0f, &NameBox, &SaveButton);
		NameBox.VSplitRight(6.0f, &NameBox, nullptr);
		Ui()->DoEditBox(&s_SkinNameInput, &NameBox, 12.0f);
		static CButtonContainer s_SaveSkinButton;
		static float s_SavedTime = -100.0f;
		static bool s_SaveOk = false;
		if(DoButton_Menu(&s_SaveSkinButton, Localize("Save and wear"), 0, &SaveButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f, 0.0f, ColorRGBA(0.45f, 0.2f, 0.75f, 0.8f)))
		{
			s_SaveOk = CClient.SkinBuilderSave();
			s_SavedTime = Client()->GlobalTime();
		}
		if(Client()->GlobalTime() - s_SavedTime < 4.0f)
			Right.Note(s_SaveOk ? Localize("Saved to your skins and put on") : Localize("Could not save the skin"), s_SaveOk ? ColorRGBA(0.5f, 1.0f, 0.6f, 1.0f) : ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Right.Note(Localize("Others only see it if they have this skin too"), ColorRGBA(0.6f, 0.6f, 0.75f, 1.0f));
	}
	Right.Gap();

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

	// ---- updates ----
	Right.Headline(Localize("Updates"));
	{
		CCClient &CClient = GameClient()->m_CClient;
		char aBuf[128];
		if(CCClient::CurrentBuild() > 0)
			str_format(aBuf, sizeof(aBuf), "%s: #%d", Localize("Your version"), CCClient::CurrentBuild());
		else
			str_copy(aBuf, Localize("Your version: local build (no updates)"));
		Right.Note(aBuf, ColorRGBA(0.75f, 0.75f, 0.9f, 1.0f));
		Right.Check(&g_Config.m_CcUpdateCheck, Localize("Check for updates when the game starts"));
		if(CClient.CanSelfUpdate())
		{
			static CButtonContainer s_CheckButton;
			CUIRect Button = Right.Line();
			Button.VSplitLeft(220.0f, &Button, nullptr);
			const CCClient::EUpdateState State = CClient.UpdateState();
			const char *pLabel = Localize("Check for updates");
			if(State == CCClient::EUpdateState::CHECKING)
				pLabel = Localize("Checking…");
			else if(State == CCClient::EUpdateState::UP_TO_DATE)
				pLabel = Localize("You have the newest version");
			else if(State == CCClient::EUpdateState::AVAILABLE)
				pLabel = Localize("Update is ready: see the main menu");
			if(DoButton_Menu(&s_CheckButton, pLabel, 0, &Button))
				CClient.CheckForUpdate();
		}
	}
	Right.Gap();

	// ---- save wheel and markers ----
	Right.Headline(Localize("Saves and markers"));
	Right.Note(Localize("Hold Shift and press Q or E: save wheel"), ColorRGBA(0.6f, 0.9f, 1.0f, 1.0f));
	Right.Note(Localize("Middle mouse button: marker for your team"), ColorRGBA(0.6f, 0.9f, 1.0f, 1.0f));
	Right.Note(Localize("Others with Cr1stal Client see pictures and markers"), ColorRGBA(0.75f, 0.75f, 0.9f, 1.0f));
	{
		static CButtonContainer s_BindMarkerButton;
		CUIRect Button = Right.Line();
		Button.VSplitLeft(220.0f, &Button, nullptr);
		if(DoButton_Menu(&s_BindMarkerButton, Localize("Put markers on the middle mouse button"), 0, &Button))
			Console()->ExecuteLine("bind mouse3 cc_ping", IConsole::CLIENT_ID_UNSPECIFIED);
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
