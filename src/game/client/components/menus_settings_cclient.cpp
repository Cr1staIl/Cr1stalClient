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
#include <map>

static constexpr float CC_LINE = 20.0f;
static constexpr float CC_HEADLINE = 20.0f;
static constexpr float CC_SECTION_GAP = 12.0f;
static constexpr float CC_TOGGLE_LINE = 24.0f;

// iPhone style switch colors: gray = off, green = on
static const ColorRGBA CC_TOGGLE_OFF = ColorRGBA(0.23f, 0.21f, 0.28f, 1.0f);
static const ColorRGBA CC_TOGGLE_ON = ColorRGBA(0.204f, 0.78f, 0.349f, 1.0f);

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
		Gap(2.0f);
		CUIRect Label = Line(CC_HEADLINE + 4.0f);
		SLabelProperties Props;
		Props.SetColor(ColorRGBA(0.73f, 0.65f, 0.86f, 1.0f));
		m_pUi->DoLabel(&Label, pText, 13.0f, TEXTALIGN_ML, Props);
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

		if(m_pUi->HotItem() == pId)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.04f), IGraphics::CORNER_ALL, 6.0f);

		CUIRect Rest, Switch;
		Row.VSplitRight(40.0f, &Rest, &Switch);
		Switch.VMargin(2.0f, &Switch);
		Switch.HMargin((Switch.h - 18.0f) / 2.0f, &Switch);
		const ColorRGBA Track(
			CC_TOGGLE_OFF.r + (CC_TOGGLE_ON.r - CC_TOGGLE_OFF.r) * A,
			CC_TOGGLE_OFF.g + (CC_TOGGLE_ON.g - CC_TOGGLE_OFF.g) * A,
			CC_TOGGLE_OFF.b + (CC_TOGGLE_ON.b - CC_TOGGLE_OFF.b) * A, 1.0f);
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
		Rest.VSplitLeft(4.0f, nullptr, &Label);
		Label.VSplitRight(6.0f, &Label, nullptr);
		SLabelProperties Props;
		Props.m_MaxWidth = Label.w;
		Props.m_EllipsisAtEnd = true;
		Props.SetColor(ColorRGBA(0.95f, 0.93f, 0.98f, 1.0f));
		m_pUi->DoLabel(&Label, pText, 12.5f, TEXTALIGN_ML, Props);

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
	// header: client icon and name
	{
		CUIRect Header;
		MainView.HSplitTop(40.0f, &Header, &MainView);
		MainView.HSplitTop(8.0f, nullptr, &MainView);
		Header.Draw(ColorRGBA(0.09f, 0.055f, 0.15f, 0.85f), IGraphics::CORNER_ALL, 12.0f);

		CUIRect Icon, Title;
		Header.VSplitLeft(8.0f, nullptr, &Header);
		Header.VSplitLeft(28.0f, &Icon, &Header);
		Icon.HMargin((Icon.h - 28.0f) / 2.0f, &Icon);
		Graphics()->TextureSet(GameClient()->m_CClient.IconTexture());
		Graphics()->QuadsBegin();
		Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
		IGraphics::CQuadItem IconQuad(Icon.x, Icon.y, Icon.w, Icon.h);
		Graphics()->QuadsDrawTL(&IconQuad, 1);
		Graphics()->QuadsEnd();

		Header.VSplitLeft(10.0f, nullptr, &Title);
		const float FontSize = 18.0f;
		TextRender()->SetFontPreset(EFontPreset::TITLE_FONT);
		const float FirstW = TextRender()->TextWidth(FontSize, "Cr\xe2\x99\xb1stal ");
		const float TextY = Title.y + (Title.h - FontSize) / 2.0f - 1.0f;
		TextRender()->TextColor(0.95f, 0.92f, 1.0f, 1.0f);
		TextRender()->Text(Title.x, TextY, FontSize, "Cr\xe2\x99\xb1stal ");
		TextRender()->TextColor(0.78f, 0.61f, 1.0f, 1.0f);
		TextRender()->Text(Title.x + FirstW, TextY, FontSize, "Client");
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		char aBuild[32];
		str_format(aBuild, sizeof(aBuild), "build #%d", GameClient()->m_CClient.CurrentBuild());
		CUIRect BuildLabel;
		Title.VSplitRight(12.0f, &BuildLabel, nullptr);
		SLabelProperties Props;
		Props.SetColor(ColorRGBA(0.6f, 0.55f, 0.7f, 1.0f));
		Ui()->DoLabel(&BuildLabel, aBuild, 11.0f, TEXTALIGN_MR, Props);
	}

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

	// ---- team ----
	Right.Headline(Localize("Team"));
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
