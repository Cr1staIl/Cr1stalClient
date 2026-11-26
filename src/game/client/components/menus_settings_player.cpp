/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/components/cclient_playtime.h>
#include <game/client/components/cclient_style.h>
#include <game/client/components/console.h>
#include <game/client/components/countryflags.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <vector>

// ---------------------------------------------------------------- C-Client profile

static ColorRGBA PlaytimeCategoryColor(int Category)
{
	static const ColorRGBA s_aColors[CCPlaytime::NUM_CATEGORIES] = {
		ColorRGBA(1.0f, 0.62f, 0.66f, 1.0f), // easy
		ColorRGBA(0.98f, 0.42f, 0.5f, 1.0f), // main
		ColorRGBA(0.95f, 0.24f, 0.37f, 1.0f), // hard
		ColorRGBA(0.86f, 0.07f, 0.23f, 1.0f), // insane
		ColorRGBA(0.64f, 0.03f, 0.16f, 1.0f), // extreme
		ColorRGBA(0.48f, 0.02f, 0.11f, 1.0f), // mods
		ColorRGBA(0.95f, 0.4f, 0.28f, 1.0f), // jet
		ColorRGBA(0.78f, 0.22f, 0.45f, 1.0f), // solo
		ColorRGBA(0.38f, 0.36f, 0.4f, 1.0f), // other
	};
	return s_aColors[std::clamp(Category, 0, CCPlaytime::NUM_CATEGORIES - 1)];
}

// part of a ring from angle A0 to A1 (radians, 0 = up, clockwise)
static void DrawRingPart(IGraphics *pGraphics, vec2 Center, float Inner, float Outer, float A0, float A1, ColorRGBA Color)
{
	if(A1 <= A0)
		return;
	pGraphics->SetColor(Color);
	const int Steps = std::max(1, (int)((A1 - A0) / 0.05f));
	IGraphics::CFreeformItem aItems[128];
	int NumItems = 0;
	for(int i = 0; i < Steps; i++)
	{
		const float B0 = A0 + (A1 - A0) * i / Steps;
		const float B1 = A0 + (A1 - A0) * (i + 1) / Steps;
		const vec2 D0(std::sin(B0), -std::cos(B0));
		const vec2 D1(std::sin(B1), -std::cos(B1));
		const vec2 I0 = Center + D0 * Inner, O0 = Center + D0 * Outer;
		const vec2 I1 = Center + D1 * Inner, O1 = Center + D1 * Outer;
		aItems[NumItems++] = IGraphics::CFreeformItem(I0.x, I0.y, O0.x, O0.y, I1.x, I1.y, O1.x, O1.y);
		if(NumItems == (int)std::size(aItems))
		{
			pGraphics->QuadsDrawFreeform(aItems, NumItems);
			NumItems = 0;
		}
	}
	if(NumItems > 0)
		pGraphics->QuadsDrawFreeform(aItems, NumItems);
}

static void DrawSmallTitle(CUi *pUi, CUIRect *pRect, const char *pText, float FontSize = 11.0f)
{
	SLabelProperties Props;
	Props.SetColor(CCStyle::ACCENT_TEXT);
	pUi->DoLabel(pRect, CCStyle::Upper(pText).c_str(), FontSize, TEXTALIGN_ML, Props);
}

void CMenus::RenderSettingsPlayer(CUIRect MainView)
{
	static CLineInput s_NameInput;
	static CLineInput s_ClanInput;

	int *pCountry;
	if(!m_Dummy)
	{
		pCountry = &g_Config.m_PlayerCountry;
		s_NameInput.SetBuffer(g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName));
		s_NameInput.SetEmptyText(Client()->PlayerName());
		s_ClanInput.SetBuffer(g_Config.m_PlayerClan, sizeof(g_Config.m_PlayerClan));
	}
	else
	{
		pCountry = &g_Config.m_ClDummyCountry;
		s_NameInput.SetBuffer(g_Config.m_ClDummyName, sizeof(g_Config.m_ClDummyName));
		s_NameInput.SetEmptyText(Client()->DummyName());
		s_ClanInput.SetBuffer(g_Config.m_ClDummyClan, sizeof(g_Config.m_ClDummyClan));
	}

	const CCPlaytime &Playtime = GameClient()->m_CPlaytime;
	const int64_t Total = Playtime.Total();
	char aBuf[128];
	CUIRect Label;

	// the page grows in when it is opened
	static int64_t s_LastShown = 0;
	static float s_Intro = 0.0f;
	const int64_t Now = time_get();
	if(Now - s_LastShown > time_freq() / 2)
		s_Intro = 0.0f;
	s_LastShown = Now;
	s_Intro = std::min(1.0f, s_Intro + std::min(Client()->RenderFrameTime(), 0.05f) * 1.4f);
	const float Intro = 1.0f - (1.0f - s_Intro) * (1.0f - s_Intro) * (1.0f - s_Intro); // ease out

	CUIRect TopRow, PlaytimeCard, CountryCard;
	MainView.HSplitTop(100.0f, &TopRow, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &MainView);
	MainView.HSplitTop(158.0f, &PlaytimeCard, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &CountryCard);

	// ---- profile card: tee, name, clan ----
	CUIRect ProfileCard, StatsCard;
	TopRow.VSplitLeft(TopRow.w * 0.5f, &ProfileCard, &StatsCard);
	ProfileCard.VSplitRight(5.0f, &ProfileCard, nullptr);
	StatsCard.VSplitLeft(5.0f, nullptr, &StatsCard);
	{
		static char s_ProfileGlowKey;
		CCStyle::DrawCard(ProfileCard, CCStyle::CARD_ROUNDING, CCStyle::CARD, 0.35f * Ui()->StateAnim(&s_ProfileGlowKey, Ui()->MouseHovered(&ProfileCard), 8.0f));
		CUIRect Content;
		ProfileCard.Margin(12.0f, &Content);

		CUIRect TeeArea;
		Content.VSplitLeft(84.0f, &TeeArea, &Content);
		Content.VSplitLeft(12.0f, nullptr, &Content);

		// tee with a soft red light behind it, looking at the mouse
		const vec2 TeePos(TeeArea.x + TeeArea.w / 2.0f, TeeArea.y + TeeArea.h / 2.0f + 4.0f);
		Graphics()->TextureClear();
		Graphics()->BlendAdditive();
		Graphics()->QuadsBegin();
		for(int Ring = 6; Ring >= 1; Ring--)
		{
			Graphics()->SetColor(CCStyle::ACCENT.WithAlpha(0.03f * Intro));
			Graphics()->DrawCircle(TeePos.x, TeePos.y, 38.0f * Ring / 6.0f, 32);
		}
		Graphics()->QuadsEnd();
		Graphics()->BlendNormal();

		CTeeRenderInfo TeeInfo;
		if(!m_Dummy)
		{
			TeeInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClPlayerSkin));
			TeeInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
		}
		else
		{
			TeeInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_ClDummySkin));
			TeeInfo.ApplyColors(g_Config.m_ClDummyUseCustomColor, g_Config.m_ClDummyColorBody, g_Config.m_ClDummyColorFeet);
		}
		TeeInfo.m_Size = 62.0f * (0.85f + 0.15f * Intro);
		vec2 Look = Ui()->MousePos() - TeePos;
		Look = length(Look) > 0.001f ? normalize(Look) : vec2(1.0f, 0.0f);
		RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, Look, TeePos);

		// player / dummy switch
		CUIRect Switch, PlayerTab, DummyTab;
		Content.HSplitTop(18.0f, &Switch, &Content);
		Switch.VSplitLeft(std::min(Switch.w, 150.0f), &Switch, nullptr);
		Switch.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_ALL, Switch.h / 2.0f);
		Switch.Margin(2.0f, &Switch);
		Switch.VSplitMid(&PlayerTab, &DummyTab);
		static CButtonContainer s_PlayerTabButton, s_DummyTabButton;
		for(int Tab = 0; Tab < 2; Tab++)
		{
			CButtonContainer *pButton = Tab == 0 ? &s_PlayerTabButton : &s_DummyTabButton;
			const CUIRect &Rect = Tab == 0 ? PlayerTab : DummyTab;
			const bool Selected = m_Dummy == (Tab == 1);
			static char s_aSelectKeys[2];
			const float SelectA = Ui()->StateAnim(&s_aSelectKeys[Tab], Selected);
			const float HoverA = Ui()->HoverAnim(pButton);
			if(HoverA > 0.01f)
				Rect.Draw(CCStyle::HOVER.WithMultipliedAlpha(HoverA), IGraphics::CORNER_ALL, Rect.h / 2.0f);
			if(SelectA > 0.01f)
				Rect.Draw(CCStyle::ACCENT.WithMultipliedAlpha(SelectA), IGraphics::CORNER_ALL, Rect.h / 2.0f);
			SLabelProperties Props;
			Props.SetColor(CCStyle::Mix(CCStyle::TEXT_DIM, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), SelectA));
			Ui()->DoLabel(&Rect, Tab == 0 ? Localize("Player") : Localize("Dummy"), 10.0f, TEXTALIGN_MC, Props);
			if(Ui()->DoButtonLogic(pButton, 0, &Rect, BUTTONFLAG_LEFT))
				m_Dummy = Tab == 1;
		}

		Content.HSplitTop(6.0f, nullptr, &Content);
		CUIRect NameRow, ClanRow;
		Content.HSplitTop(22.0f, &NameRow, &Content);
		Content.HSplitTop(5.0f, nullptr, &Content);
		Content.HSplitTop(22.0f, &ClanRow, &Content);
		for(int Field = 0; Field < 2; Field++)
		{
			CUIRect Row = Field == 0 ? NameRow : ClanRow;
			Row.VSplitLeft(48.0f, &Label, &Row);
			SLabelProperties Props;
			Props.SetColor(CCStyle::TEXT_DIM);
			Ui()->DoLabel(&Label, Field == 0 ? Localize("Name") : Localize("Clan"), 11.0f, TEXTALIGN_ML, Props);
			if(Ui()->DoEditBox(Field == 0 ? &s_NameInput : &s_ClanInput, &Row, 13.0f))
				SetNeedSendInfo();
		}

		if(Client()->State() == IClient::STATE_ONLINE &&
			GameClient()->m_aNextChangeInfo[m_Dummy] > Client()->GameTick(m_Dummy))
		{
			char aTimeLeft[32];
			str_format(aTimeLeft, sizeof(aTimeLeft), Localize("%ds left"), (GameClient()->m_aNextChangeInfo[m_Dummy] - Client()->GameTick(m_Dummy) + Client()->GameTickSpeed() - 1) / Client()->GameTickSpeed());
			str_format(aBuf, sizeof(aBuf), "%s: %s", Localize("Player info change cooldown"), aTimeLeft);
			Content.HSplitTop(14.0f, &Label, &Content);
			SLabelProperties Props;
			Props.SetColor(CCStyle::TEXT_DIM);
			Ui()->DoLabel(&Label, aBuf, 9.0f, TEXTALIGN_ML, Props);
		}
	}

	// ---- quick stats ----
	{
		const std::vector<const CCPlaytime::SMap *> vpTop = Playtime.TopMaps(5);
		char aTotal[32], aMaps[16], aPoints[16];
		CCPlaytime::FormatTime(Total, aTotal, sizeof(aTotal));
		str_format(aMaps, sizeof(aMaps), "%d", Playtime.NumFinished());
		str_format(aPoints, sizeof(aPoints), "%d", Playtime.Points());
		const char *apValues[4] = {aTotal, aMaps, aPoints, vpTop.empty() ? "-" : vpTop[0]->m_Name.c_str()};
		const char *apNames[4] = {Localize("In game"), Localize("Maps finished"), Localize("Points"), Localize("Favourite map")};
		static char s_aStatKeys[4];
		for(int i = 0; i < 4; i++)
		{
			CUIRect Tile;
			const float TileW = (StatsCard.w - 8.0f * (3 - i)) / (4 - i);
			StatsCard.VSplitLeft(TileW, &Tile, &StatsCard);
			if(i < 3)
				StatsCard.VSplitLeft(8.0f, nullptr, &StatsCard);
			const float HoverA = Ui()->StateAnim(&s_aStatKeys[i], Ui()->MouseHovered(&Tile), 10.0f);
			CCStyle::DrawCard(Tile, CCStyle::CARD_ROUNDING, CCStyle::CARD, 0.5f * HoverA);
			CUIRect Inner;
			Tile.Margin(12.0f, &Inner);
			Inner.HSplitTop(14.0f, &Label, &Inner);
			DrawSmallTitle(Ui(), &Label, apNames[i], 9.5f);
			SLabelProperties Props;
			Props.m_MaxWidth = Inner.w;
			Props.m_EllipsisAtEnd = true;
			Props.SetColor(CCStyle::TEXT);
			Inner.y -= 3.0f * HoverA; // lifts a little on hover
			Ui()->DoLabel(&Inner, apValues[i], i == 3 ? 15.0f : 22.0f, TEXTALIGN_ML, Props);
		}
	}

	// ---- playtime ----
	{
		static char s_PlaytimeGlowKey;
		CCStyle::DrawCard(PlaytimeCard, CCStyle::CARD_ROUNDING, CCStyle::CARD, 0.25f * Ui()->StateAnim(&s_PlaytimeGlowKey, Ui()->MouseHovered(&PlaytimeCard), 8.0f));
		CUIRect Content, Header;
		PlaytimeCard.Margin(12.0f, &Content);
		Content.HSplitTop(18.0f, &Header, &Content);
		Content.HSplitTop(6.0f, nullptr, &Content);
		{
			CUIRect Title;
			Header.VSplitLeft(80.0f, &Title, &Header);
			DrawSmallTitle(Ui(), &Title, Localize("Playtime"), 14.0f);
		}

		CUIRect Ring, Tiles, MapList;
		Content.VSplitLeft(Content.h + 4.0f, &Ring, &Content);
		Content.VSplitLeft(14.0f, nullptr, &Content);
		Content.VSplitRight(std::min(220.0f, Content.w * 0.38f), &Tiles, &MapList);
		Tiles.VSplitRight(14.0f, &Tiles, nullptr);

		// ring: every category its own piece, it sweeps around when the page opens
		{
			const vec2 Center(Ring.x + Ring.w / 2.0f, Ring.y + Ring.h / 2.0f);
			const float Outer = std::min(Ring.w, Ring.h) / 2.0f;
			const float Inner = Outer * 0.78f;
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			DrawRingPart(Graphics(), Center, Inner, Outer, 0.0f, 2.0f * pi, ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f));
			float Angle = 0.0f;
			const float Sweep = 2.0f * pi * Intro;
			for(int Category = 0; Category < CCPlaytime::NUM_CATEGORIES && Total > 0; Category++)
			{
				const float Part = 2.0f * pi * (float)Playtime.CategorySeconds(Category) / (float)Total;
				const float A0 = Angle, A1 = std::min(Angle + Part, Sweep);
				DrawRingPart(Graphics(), Center, Inner, Outer, A0, A1, PlaytimeCategoryColor(Category));
				Angle += Part;
			}
			Graphics()->QuadsEnd();

			char aTotal[32];
			CCPlaytime::FormatTime((int64_t)(Total * Intro), aTotal, sizeof(aTotal));
			CUIRect TotalLabel, InGame;
			TotalLabel.x = Center.x - Inner;
			TotalLabel.w = Inner * 2.0f;
			TotalLabel.y = Center.y - 14.0f;
			TotalLabel.h = 18.0f;
			InGame = TotalLabel;
			InGame.y += 18.0f;
			InGame.h = 10.0f;
			Ui()->DoLabel(&TotalLabel, aTotal, 16.0f, TEXTALIGN_MC);
			SLabelProperties Props;
			Props.SetColor(CCStyle::ACCENT_TEXT);
			Ui()->DoLabel(&InGame, CCStyle::Upper(Localize("In game")).c_str(), 8.0f, TEXTALIGN_MC, Props);
		}

		// category tiles, 4 x 2
		{
			static char s_aTileKeys[8];
			CUIRect aRows[2];
			Tiles.HSplitMid(&aRows[0], &aRows[1], 8.0f);
			for(int Category = 0; Category < 8; Category++)
			{
				CUIRect &Row = aRows[Category / 4];
				CUIRect Tile;
				const int InRow = Category % 4;
				Row.VSplitLeft((Row.w - (3 - InRow) * 8.0f) / (4 - InRow), &Tile, &Row);
				if(InRow < 3)
					Row.VSplitLeft(8.0f, nullptr, &Row);
				const int64_t Seconds = Playtime.CategorySeconds(Category);
				const float Share = Total > 0 ? (float)Seconds / (float)Total : 0.0f;
				const float HoverA = Ui()->StateAnim(&s_aTileKeys[Category], Ui()->MouseHovered(&Tile), 12.0f);
				// tiles you played get a red tint, the stronger the more you played
				const ColorRGBA Base = Seconds > 0 ? CCStyle::Mix(ColorRGBA(0.12f, 0.06f, 0.08f, 1.0f), ColorRGBA(0.3f, 0.04f, 0.09f, 1.0f), std::min(1.0f, Share * 2.0f)) : ColorRGBA(0.1f, 0.095f, 0.105f, 1.0f);
				CCStyle::DrawCard(Tile, 7.0f, Base, Seconds > 0 ? 0.25f + 0.5f * HoverA : 0.4f * HoverA);
				CUIRect Inner, Name, Value;
				Tile.Margin(vec2(8.0f, 6.0f), &Inner);
				Inner.HSplitMid(&Name, &Value);
				str_format(aBuf, sizeof(aBuf), "%s", CCStyle::Upper(Localize(CCPlaytime::CategoryName(Category), "Playtime category")).c_str());
				SLabelProperties NameProps;
				NameProps.m_MaxWidth = Name.w;
				NameProps.m_EllipsisAtEnd = true;
				NameProps.SetColor(CCStyle::TEXT);
				Ui()->DoLabel(&Name, aBuf, 9.0f, TEXTALIGN_ML, NameProps);
				char aTime[32];
				CCPlaytime::FormatTime(Seconds, aTime, sizeof(aTime));
				str_format(aBuf, sizeof(aBuf), "%s  %d%%", aTime, round_to_int(Share * 100.0f));
				SLabelProperties ValueProps;
				ValueProps.m_MaxWidth = Value.w;
				ValueProps.m_EllipsisAtEnd = true;
				ValueProps.SetColor(PlaytimeCategoryColor(Category));
				Ui()->DoLabel(&Value, aBuf, 9.0f, TEXTALIGN_ML, ValueProps);
			}
		}

		// maps you spent the most time on
		{
			CUIRect Divider = MapList;
			Divider.w = 0.6f;
			Divider.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f), IGraphics::CORNER_NONE, 0.0f);
			MapList.VSplitLeft(12.0f, nullptr, &MapList);
			MapList.HSplitTop(16.0f, &Label, &MapList);
			DrawSmallTitle(Ui(), &Label, Localize("Most time spent on maps"), 9.0f);
			const std::vector<const CCPlaytime::SMap *> vpTop = Playtime.TopMaps(5);
			if(vpTop.empty())
			{
				MapList.HSplitTop(18.0f, &Label, &MapList);
				SLabelProperties Props;
				Props.SetColor(CCStyle::TEXT_DIM);
				Ui()->DoLabel(&Label, Localize("Play a map and it shows up here"), 10.0f, TEXTALIGN_ML, Props);
			}
			static char s_aRowKeys[5];
			for(int i = 0; i < (int)vpTop.size(); i++)
			{
				CUIRect Row, Number, Time;
				MapList.HSplitTop(std::min(21.0f, MapList.h / (vpTop.size() - i)), &Row, &MapList);
				const float HoverA = Ui()->StateAnim(&s_aRowKeys[i], Ui()->MouseHovered(&Row), 12.0f);
				if(HoverA > 0.01f)
					Row.Draw(CCStyle::ACCENT.WithAlpha(0.12f * HoverA), IGraphics::CORNER_ALL, 5.0f);
				// rows slide in one after another
				const float RowIntro = std::clamp(Intro * 1.6f - i * 0.12f, 0.0f, 1.0f);
				Row.x += (1.0f - RowIntro) * 12.0f + 3.0f * HoverA;
				Row.VSplitLeft(18.0f, &Number, &Row);
				Row.VSplitRight(50.0f, &Row, &Time);
				str_format(aBuf, sizeof(aBuf), "%02d", i + 1);
				SLabelProperties NumberProps;
				NumberProps.SetColor(CCStyle::ACCENT_TEXT.WithAlpha(0.75f * RowIntro));
				Ui()->DoLabel(&Number, aBuf, 8.5f, TEXTALIGN_ML, NumberProps);
				SLabelProperties NameProps;
				NameProps.m_MaxWidth = Row.w;
				NameProps.m_EllipsisAtEnd = true;
				NameProps.SetColor(CCStyle::TEXT.WithAlpha(RowIntro));
				Ui()->DoLabel(&Row, vpTop[i]->m_Name.c_str(), 11.0f, TEXTALIGN_ML, NameProps);
				char aTime[32];
				CCPlaytime::FormatTime(vpTop[i]->m_Seconds, aTime, sizeof(aTime));
				SLabelProperties TimeProps;
				TimeProps.SetColor(CCStyle::ACCENT_TEXT.WithAlpha(RowIntro));
				Ui()->DoLabel(&Time, aTime, 9.5f, TEXTALIGN_MR, TimeProps);
			}
		}
	}

	// ---- country ----
	CCStyle::DrawCard(CountryCard);
	CountryCard.Margin(12.0f, &MainView);
	MainView.HSplitTop(16.0f, &Label, &MainView);
	DrawSmallTitle(Ui(), &Label, Localize("Country"), 11.0f);
	MainView.HSplitTop(6.0f, nullptr, &MainView);
	CUIRect QuickSearch;

	// country flag selector
	static CLineInputBuffered<25> s_FlagFilterInput;

	class CCountryFlagEntry
	{
	public:
		const CCountryFlags::CCountryFlag *m_pFlag;
		std::optional<std::pair<int, int>> m_NameMatch;
	};
	std::vector<CCountryFlagEntry> vFilteredFlags;
	for(size_t i = 0; i < GameClient()->m_CountryFlags.Num(); ++i)
	{
		const CCountryFlags::CCountryFlag &Entry = GameClient()->m_CountryFlags.GetByIndex(i);
		if(!s_FlagFilterInput.IsEmpty())
		{
			const char *pNameMatchEnd;
			const char *pNameMatchStart = str_utf8_find_nocase(Entry.m_aCountryCodeString, s_FlagFilterInput.GetString(), &pNameMatchEnd);
			if(pNameMatchStart != nullptr)
			{
				vFilteredFlags.emplace_back(&Entry, std::make_pair<int, int>(pNameMatchStart - Entry.m_aCountryCodeString, pNameMatchEnd - pNameMatchStart));
			}
		}
		else
		{
			vFilteredFlags.emplace_back(&Entry, std::nullopt);
		}
	}

	MainView.HSplitBottom(20.0f, &MainView, &QuickSearch);
	MainView.HSplitBottom(5.0f, &MainView, nullptr);
	QuickSearch.VSplitLeft(220.0f, &QuickSearch, nullptr);

	int OldSelected = -1;
	static CListBox s_ListBox;
	s_ListBox.DoStart(48.0f, vFilteredFlags.size(), 10, 3, OldSelected, &MainView);

	for(size_t i = 0; i < vFilteredFlags.size(); i++)
	{
		const CCountryFlagEntry &Entry = vFilteredFlags[i];

		if(Entry.m_pFlag->m_CountryCode == *pCountry)
			OldSelected = i;

		const CListboxItem Item = s_ListBox.DoNextItem(&Entry.m_pFlag->m_CountryCode, OldSelected >= 0 && (size_t)OldSelected == i);
		if(!Item.m_Visible)
			continue;

		CUIRect FlagRect;
		Item.m_Rect.Margin(5.0f, &FlagRect);
		FlagRect.HSplitBottom(12.0f, &FlagRect, &Label);
		Label.HSplitTop(2.0f, nullptr, &Label);
		const float OldWidth = FlagRect.w;
		FlagRect.w = FlagRect.h * 2;
		FlagRect.x += (OldWidth - FlagRect.w) / 2.0f;
		GameClient()->m_CountryFlags.Render(Entry.m_pFlag->m_CountryCode, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), FlagRect.x, FlagRect.y, FlagRect.w, FlagRect.h);

		if(Entry.m_pFlag->m_Texture.IsValid() || Entry.m_pFlag->m_CountryCode == CountryCode::DEFAULT)
		{
			SLabelProperties Props;
			Props.m_MaxWidth = Label.w - 5.0f;
			if(Entry.m_NameMatch.has_value())
			{
				const auto [MatchStart, MatchLength] = Entry.m_NameMatch.value();
				Props.m_vColorSplits.emplace_back(MatchStart, MatchLength, ColorRGBA(0.4f, 0.4f, 1.0f, 1.0f));
			}
			Ui()->DoLabel(&Label, Entry.m_pFlag->m_aCountryCodeString, 10.0f, TEXTALIGN_MC, Props);
		}
	}

	const int NewSelected = s_ListBox.DoEnd();
	if(OldSelected != NewSelected)
	{
		*pCountry = vFilteredFlags[NewSelected].m_pFlag->m_CountryCode;
		SetNeedSendInfo();
	}

	if(GameClient()->m_CountryFlags.Num() > 0 && vFilteredFlags.empty())
	{
		CUIRect FilterLabel, ResetButton;
		MainView.HMargin((MainView.h - (16.0f + 18.0f + 8.0f)) / 2.0f, &FilterLabel);
		FilterLabel.HSplitTop(16.0f, &FilterLabel, &ResetButton);
		ResetButton.HSplitTop(8.0f, nullptr, &ResetButton);
		ResetButton.VMargin((ResetButton.w - 200.0f) / 2.0f, &ResetButton);
		Ui()->DoLabel(&FilterLabel, Localize("No country flags match your filter criteria"), 16.0f, TEXTALIGN_MC);
		static CButtonContainer s_ResetButton;
		if(DoButton_Menu(&s_ResetButton, Localize("Reset filter"), 0, &ResetButton))
		{
			s_FlagFilterInput.Clear();
		}
	}

	Ui()->DoEditBox_Search(&s_FlagFilterInput, &QuickSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());
}
