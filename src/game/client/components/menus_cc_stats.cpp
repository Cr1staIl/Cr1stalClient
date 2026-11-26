/* Cr1stal Client: statistics of all Cr1stal Client players. Based on DDNet, see licence.txt. */
#include "cclient_playtime.h"
#include "cclient_presence.h"
#include "cclient_style.h"
#include "menus.h"

#include <base/str.h>
#include <base/time.h>

#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>

enum
{
	CC_SORT_HOURS = 0,
	CC_SORT_MAPS,
	CC_SORT_POINTS,
};

void CMenus::CcStatsFetch()
{
	if(m_pCcStatsRequest)
		return;
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%s/s.json", CCPresence::DatabaseUrl());
	m_pCcStatsRequest = HttpGet(aUrl);
	m_pCcStatsRequest->Timeout(CTimeout{5000, 20000, 500, 10});
	m_pCcStatsRequest->MaxResponseSize(8 * 1024 * 1024);
	m_pCcStatsRequest->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pCcStatsRequest);
	m_CcStatsFetched = time_get();
}

void CMenus::CcStatsRead()
{
	// your own numbers, always fresh
	const CCPlaytime &Playtime = GameClient()->m_CPlaytime;
	SCcStatsEntry Me;
	Me.m_Name = g_Config.m_PlayerName[0] ? g_Config.m_PlayerName : Client()->PlayerName();
	Me.m_Clan = g_Config.m_PlayerClan;
	Me.m_Seconds = Playtime.Total();
	Me.m_Finished = Playtime.NumFinished();
	Me.m_Played = Playtime.NumMaps();
	Me.m_Points = Playtime.Points();
	for(int i = 0; i < CCPlaytime::NUM_CATEGORIES && i < 9; i++)
		Me.m_aCategories[i] = Playtime.CategorySeconds(i);
	for(const CCPlaytime::SMap *pMap : Playtime.TopMaps(3))
	{
		if(!Me.m_TopMaps.empty())
			Me.m_TopMaps += "|";
		Me.m_TopMaps += pMap->m_Name;
	}
	Me.m_Me = true;

	if(m_pCcStatsRequest && m_pCcStatsRequest->Done())
	{
		std::shared_ptr<IHttpRequest> pRequest = m_pCcStatsRequest;
		m_pCcStatsRequest = nullptr;
		json_value *pJson = pRequest->State() == EHttpState::DONE ? pRequest->ResultJson() : nullptr;
		if(pJson && pJson->type == json_object)
		{
			m_vCcStats.clear();
			for(unsigned i = 0; i < pJson->u.object.length; i++)
			{
				const json_value *pEntry = pJson->u.object.values[i].value;
				const json_value *pName = json_object_get(pEntry, "n");
				if(pName == &json_value_none || pName->type != json_string)
					continue;
				SCcStatsEntry Entry;
				Entry.m_Name = pName->u.string.ptr;
				auto Number = [&](const char *pKey) -> int64_t {
					const json_value *pValue = json_object_get(pEntry, pKey);
					return pValue != &json_value_none && pValue->type == json_integer ? (int64_t)pValue->u.integer : 0;
				};
				auto Text = [&](const char *pKey) -> std::string {
					const json_value *pValue = json_object_get(pEntry, pKey);
					return pValue != &json_value_none && pValue->type == json_string ? pValue->u.string.ptr : "";
				};
				Entry.m_Clan = Text("cl");
				Entry.m_Seconds = Number("h");
				Entry.m_Finished = (int)Number("m");
				Entry.m_Played = (int)Number("pl");
				Entry.m_Points = (int)Number("p");
				Entry.m_TopMaps = Text("f");
				const std::string Categories = Text("c");
				int Index = 0;
				for(const char *p = Categories.c_str(); *p && Index < 9; Index++)
				{
					Entry.m_aCategories[Index] = str_toint64_base(p);
					const char *pComma = str_find(p, ",");
					if(!pComma)
						break;
					p = pComma + 1;
				}
				m_vCcStats.push_back(Entry);
			}
		}
		if(pJson)
			json_value_free(pJson);
	}

	// put yourself in (or update your row)
	bool Found = false;
	for(SCcStatsEntry &Entry : m_vCcStats)
	{
		if(Entry.m_Me || Entry.m_Name == Me.m_Name)
		{
			Entry = Me;
			Found = true;
		}
	}
	if(!Found)
		m_vCcStats.push_back(Me);
}

void CMenus::RenderCcStats(CUIRect MainView)
{
	if(!m_pCcStatsRequest && (m_CcStatsFetched == 0 || time_get() > m_CcStatsFetched + time_freq() * 60))
		CcStatsFetch();
	CcStatsRead();

	// newest sort
	std::vector<const SCcStatsEntry *> vpSorted;
	for(const SCcStatsEntry &Entry : m_vCcStats)
		vpSorted.push_back(&Entry);
	std::stable_sort(vpSorted.begin(), vpSorted.end(), [this](const SCcStatsEntry *a, const SCcStatsEntry *b) {
		if(m_CcStatsSort == CC_SORT_MAPS && a->m_Finished != b->m_Finished)
			return a->m_Finished > b->m_Finished;
		if(m_CcStatsSort == CC_SORT_POINTS && a->m_Points != b->m_Points)
			return a->m_Points > b->m_Points;
		return a->m_Seconds > b->m_Seconds;
	});

	MainView.Margin(14.0f, &MainView);
	CUIRect ListSide, ProfileSide;
	MainView.VSplitRight(std::min(330.0f, MainView.w * 0.38f), &ListSide, &ProfileSide);
	ListSide.VSplitRight(12.0f, &ListSide, nullptr);

	// ---- top bar: title, search, sort ----
	CUIRect Bar, Title, Search, SortRect;
	ListSide.HSplitTop(28.0f, &Bar, &ListSide);
	ListSide.HSplitTop(10.0f, nullptr, &ListSide);
	Bar.VSplitLeft(150.0f, &Title, &Bar);
	{
		SLabelProperties Props;
		Props.SetColor(CCStyle::ACCENT_TEXT);
		Ui()->DoLabel(&Title, CCStyle::Upper(Localize("Statistics")).c_str(), 16.0f, TEXTALIGN_ML, Props);
	}
	Bar.VSplitRight(250.0f, &Search, &SortRect);
	Search.VSplitRight(10.0f, &Search, nullptr);
	static CLineInput s_SearchInput(m_aCcStatsSearch, sizeof(m_aCcStatsSearch));
	s_SearchInput.SetEmptyText(Localize("Search a player"));
	Search.HMargin(3.0f, &Search);
	Ui()->DoClearableEditBox(&s_SearchInput, &Search, 12.0f);
	{
		static CButtonContainer s_aSortButtons[3];
		const char *apSorts[] = {Localize("Hours"), Localize("Maps"), Localize("Points")};
		SortRect.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_ALL, SortRect.h / 2.0f);
		for(int i = 0; i < 3; i++)
		{
			CUIRect Button;
			SortRect.VSplitLeft(SortRect.w / (3 - i), &Button, &SortRect);
			const bool Active = m_CcStatsSort == i;
			const float Hover = Ui()->HoverAnim(&s_aSortButtons[i]);
			if(Active || Hover > 0.0f)
				Button.Draw(Active ? CCStyle::ACCENT : ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f * Hover), IGraphics::CORNER_ALL, Button.h / 2.0f);
			if(Ui()->DoButtonLogic(&s_aSortButtons[i], 0, &Button, BUTTONFLAG_LEFT))
				m_CcStatsSort = i;
			Ui()->DoLabel(&Button, apSorts[i], 11.0f, TEXTALIGN_MC);
		}
	}

	// ---- the list ----
	CCStyle::DrawCard(ListSide, CCStyle::CARD_ROUNDING);
	CUIRect Head;
	ListSide.Margin(8.0f, &ListSide);
	ListSide.HSplitTop(18.0f, &Head, &ListSide);
	const float ColRank = 34.0f, ColNum = 70.0f;
	{
		SLabelProperties Props;
		Props.SetColor(CCStyle::TEXT_DIM);
		CUIRect Col;
		Head.VSplitLeft(ColRank, &Col, &Head);
		Ui()->DoLabel(&Col, "#", 10.0f, TEXTALIGN_MC, Props);
		CUIRect Num3, Num2, Num1;
		Head.VSplitRight(ColNum, &Head, &Num3);
		Head.VSplitRight(ColNum, &Head, &Num2);
		Head.VSplitRight(ColNum, &Head, &Num1);
		Ui()->DoLabel(&Head, Localize("Player"), 10.0f, TEXTALIGN_ML, Props);
		Ui()->DoLabel(&Num1, Localize("Hours"), 10.0f, TEXTALIGN_MC, Props);
		Ui()->DoLabel(&Num2, Localize("Maps"), 10.0f, TEXTALIGN_MC, Props);
		Ui()->DoLabel(&Num3, Localize("Points"), 10.0f, TEXTALIGN_MC, Props);
	}

	std::vector<std::pair<int, const SCcStatsEntry *>> vpVisible;
	for(size_t i = 0; i < vpSorted.size(); i++)
		if(!m_aCcStatsSearch[0] || str_find_nocase(vpSorted[i]->m_Name.c_str(), m_aCcStatsSearch) || str_find_nocase(vpSorted[i]->m_Clan.c_str(), m_aCcStatsSearch))
			vpVisible.emplace_back((int)i + 1, vpSorted[i]);

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	s_ScrollRegion.Begin(&ListSide, &ScrollParams);
	static std::vector<CButtonContainer> s_vRowButtons;
	s_vRowButtons.resize(std::max(s_vRowButtons.size(), vpVisible.size()));
	const SCcStatsEntry *pSelected = nullptr;
	for(size_t i = 0; i < vpVisible.size(); i++)
	{
		const SCcStatsEntry *pEntry = vpVisible[i].second;
		CUIRect Row;
		ListSide.HSplitTop(24.0f, &Row, &ListSide);
		ListSide.HSplitTop(3.0f, nullptr, &ListSide);
		const bool Visible = s_ScrollRegion.AddRect(Row);
		if(pEntry->m_Name == m_CcStatsSelected)
			pSelected = pEntry;
		if(!Visible)
			continue;
		const bool Selected = pEntry->m_Name == m_CcStatsSelected;
		const float Hover = Ui()->HoverAnim(&s_vRowButtons[i]);
		ColorRGBA Bg = ColorRGBA(1.0f, 1.0f, 1.0f, 0.03f + 0.05f * Hover);
		if(pEntry->m_Me)
			Bg = CCStyle::ACCENT.WithAlpha(0.18f + 0.08f * Hover);
		if(Selected)
			Bg = CCStyle::ACCENT.WithAlpha(0.45f);
		Row.Draw(Bg, IGraphics::CORNER_ALL, 7.0f);
		if(Ui()->DoButtonLogic(&s_vRowButtons[i], 0, &Row, BUTTONFLAG_LEFT))
			m_CcStatsSelected = pEntry->m_Name;

		CUIRect Col, Num1, Num2, Num3;
		Row.VSplitLeft(ColRank, &Col, &Row);
		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), "%d", vpVisible[i].first);
		SLabelProperties RankProps;
		RankProps.SetColor(vpVisible[i].first <= 3 ? CCStyle::ACCENT_TEXT : CCStyle::TEXT_DIM);
		Ui()->DoLabel(&Col, aBuf, 12.0f, TEXTALIGN_MC, RankProps);
		Row.VSplitRight(ColNum, &Row, &Num3);
		Row.VSplitRight(ColNum, &Row, &Num2);
		Row.VSplitRight(ColNum, &Row, &Num1);
		SLabelProperties NameProps;
		NameProps.m_MaxWidth = Row.w;
		NameProps.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Row, pEntry->m_Name.c_str(), 12.5f, TEXTALIGN_ML, NameProps);
		CCPlaytime::FormatTime(pEntry->m_Seconds, aBuf, sizeof(aBuf));
		Ui()->DoLabel(&Num1, aBuf, 11.0f, TEXTALIGN_MC);
		str_format(aBuf, sizeof(aBuf), "%d", pEntry->m_Finished);
		Ui()->DoLabel(&Num2, aBuf, 11.0f, TEXTALIGN_MC);
		str_format(aBuf, sizeof(aBuf), "%d", pEntry->m_Points);
		Ui()->DoLabel(&Num3, aBuf, 11.0f, TEXTALIGN_MC);
	}
	if(vpVisible.empty())
	{
		CUIRect Empty;
		ListSide.HSplitTop(30.0f, &Empty, &ListSide);
		SLabelProperties Props;
		Props.SetColor(CCStyle::TEXT_DIM);
		Ui()->DoLabel(&Empty, m_pCcStatsRequest ? Localize("Loading...") : Localize("No players found"), 12.0f, TEXTALIGN_MC, Props);
	}
	s_ScrollRegion.End();

	// ---- profile of the selected player (yourself when nothing is selected) ----
	if(!pSelected)
		for(const SCcStatsEntry &Entry : m_vCcStats)
			if(Entry.m_Me)
				pSelected = &Entry;
	CCStyle::DrawCard(ProfileSide, CCStyle::CARD_ROUNDING);
	if(!pSelected)
		return;
	CUIRect Content;
	ProfileSide.Margin(16.0f, &Content);
	CUIRect Line;
	Content.HSplitTop(26.0f, &Line, &Content);
	{
		SLabelProperties Props;
		Props.m_MaxWidth = Line.w;
		Props.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Line, pSelected->m_Name.c_str(), 20.0f, TEXTALIGN_ML, Props);
	}
	if(!pSelected->m_Clan.empty())
	{
		Content.HSplitTop(16.0f, &Line, &Content);
		SLabelProperties Props;
		Props.SetColor(CCStyle::TEXT_DIM);
		Ui()->DoLabel(&Line, pSelected->m_Clan.c_str(), 12.0f, TEXTALIGN_ML, Props);
	}
	Content.HSplitTop(12.0f, nullptr, &Content);

	// three tiles
	{
		CUIRect Tiles;
		Content.HSplitTop(54.0f, &Tiles, &Content);
		Content.HSplitTop(14.0f, nullptr, &Content);
		char aTime[32], aMaps[16], aPoints[16];
		CCPlaytime::FormatTime(pSelected->m_Seconds, aTime, sizeof(aTime));
		str_format(aMaps, sizeof(aMaps), "%d", pSelected->m_Finished);
		str_format(aPoints, sizeof(aPoints), "%d", pSelected->m_Points);
		const char *apValues[3] = {aTime, aMaps, aPoints};
		const char *apNames[3] = {Localize("In game"), Localize("Maps finished"), Localize("Points")};
		for(int i = 0; i < 3; i++)
		{
			CUIRect Tile, Label;
			Tiles.VSplitLeft((Tiles.w - 8.0f * (2 - i)) / (3 - i), &Tile, &Tiles);
			if(i < 2)
				Tiles.VSplitLeft(8.0f, nullptr, &Tiles);
			Tile.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_ALL, 9.0f);
			Tile.Margin(8.0f, &Tile);
			Tile.HSplitTop(12.0f, &Label, &Tile);
			SLabelProperties Props;
			Props.SetColor(CCStyle::TEXT_DIM);
			Ui()->DoLabel(&Label, CCStyle::Upper(apNames[i]).c_str(), 8.0f, TEXTALIGN_ML, Props);
			Ui()->DoLabel(&Tile, apValues[i], 16.0f, TEXTALIGN_ML);
		}
	}

	// time per category as bars
	{
		Content.HSplitTop(16.0f, &Line, &Content);
		SLabelProperties Props;
		Props.SetColor(CCStyle::ACCENT_TEXT);
		Ui()->DoLabel(&Line, CCStyle::Upper(Localize("Playtime")).c_str(), 10.0f, TEXTALIGN_ML, Props);
		int64_t Max = 1;
		for(int64_t Seconds : pSelected->m_aCategories)
			Max = std::max(Max, Seconds);
		for(int i = 0; i < CCPlaytime::NUM_CATEGORIES && i < 9; i++)
		{
			if(pSelected->m_aCategories[i] <= 0)
				continue;
			CUIRect Row, Name, Bar, Value;
			Content.HSplitTop(17.0f, &Row, &Content);
			Row.VSplitLeft(62.0f, &Name, &Row);
			Row.VSplitRight(58.0f, &Bar, &Value);
			Ui()->DoLabel(&Name, CCPlaytime::CategoryName(i), 11.0f, TEXTALIGN_ML);
			Bar.HMargin(6.0f, &Bar);
			Bar.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_ALL, Bar.h / 2.0f);
			CUIRect Fill = Bar;
			Fill.w = std::max(Bar.h, Bar.w * pSelected->m_aCategories[i] / (float)Max);
			Fill.Draw(CCStyle::ACCENT, IGraphics::CORNER_ALL, Bar.h / 2.0f);
			char aBuf[32];
			CCPlaytime::FormatTime(pSelected->m_aCategories[i], aBuf, sizeof(aBuf));
			Ui()->DoLabel(&Value, aBuf, 10.0f, TEXTALIGN_MR);
		}
		Content.HSplitTop(12.0f, nullptr, &Content);
	}

	// favourite maps
	if(!pSelected->m_TopMaps.empty())
	{
		Content.HSplitTop(16.0f, &Line, &Content);
		SLabelProperties Props;
		Props.SetColor(CCStyle::ACCENT_TEXT);
		Ui()->DoLabel(&Line, CCStyle::Upper(Localize("Most played maps")).c_str(), 10.0f, TEXTALIGN_ML, Props);
		std::string Rest = pSelected->m_TopMaps;
		int Place = 1;
		while(!Rest.empty() && Place <= 3)
		{
			const size_t Bar = Rest.find('|');
			const std::string Map = Rest.substr(0, Bar);
			Rest = Bar == std::string::npos ? "" : Rest.substr(Bar + 1);
			char aBuf[160];
			str_format(aBuf, sizeof(aBuf), "%d.  %s", Place++, Map.c_str());
			Content.HSplitTop(17.0f, &Line, &Content);
			Ui()->DoLabel(&Line, aBuf, 11.5f, TEXTALIGN_ML);
		}
	}
}
