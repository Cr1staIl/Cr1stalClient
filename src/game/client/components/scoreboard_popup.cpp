/* Cr1stal Client: the menu that opens when you click a player in the scoreboard. Based on DDNet, see licence.txt. */
#include "cclient_neon.h"
#include "scoreboard.h"

#include <base/str.h>

#include <engine/console.h>
#include <engine/font_icons.h>
#include <engine/http.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/animstate.h>
#include <game/client/components/bestclient/bestclient.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace
{
// the popup draws its own card over the whole popup area, the border and the margin of the popup are 1 + 4
constexpr float POPUP_INSET = 5.0f;

constexpr float PAD = 8.0f;
constexpr float HEADER_H = 30.0f;
constexpr float GAP = 6.0f;
constexpr float ICON_H = 22.0f;
constexpr float ICON_GAP = 4.0f;
constexpr float BTN_H = 18.0f;
constexpr float BTN_GAP = 3.0f;
constexpr float LABEL_H = 11.0f;
constexpr float FONT = 10.5f;

struct SLayout
{
	bool m_Local = false;
	bool m_Spectate = false;
	bool m_Voice = false;
	int m_NumGrid = 0;
	bool m_TeamExit = false;
	bool m_TeamJoin = false;
	bool m_TeamInvite = false;
	bool m_TeamKick = false;
	bool m_TeamLock = false;
	int m_NumTeam = 0;
};

SLayout MakeLayout(const CGameClient *pGameClient, bool IsLocal, bool IsSpectatorRow, int ClientId)
{
	SLayout L;
	L.m_Local = IsLocal;
	L.m_Spectate = !IsSpectatorRow;
	L.m_Voice = !IsLocal && g_Config.m_BcVoiceChatEnable;
	// profile, find hours, clip name; others also get copy skin, whisper, vote kick, swap
	L.m_NumGrid = 3 + (IsLocal ? 0 : 4) + (L.m_Voice ? 1 : 0);

	const int LocalId = pGameClient->m_aLocalIds[g_Config.m_ClDummy];
	const int LocalTeam = LocalId >= 0 && LocalId < MAX_CLIENTS ? pGameClient->m_Teams.Team(LocalId) : TEAM_FLOCK;
	const int TargetTeam = ClientId >= 0 && ClientId < MAX_CLIENTS ? pGameClient->m_Teams.Team(ClientId) : TEAM_FLOCK;
	const bool LocalInTeam = LocalTeam != TEAM_FLOCK && LocalTeam != TEAM_SUPER;
	const bool TargetInTeam = TargetTeam != TEAM_FLOCK && TargetTeam != TEAM_SUPER;
	const bool LocalIsTarget = LocalId == ClientId;
	if(LocalInTeam || TargetInTeam)
	{
		L.m_TeamExit = LocalInTeam && LocalTeam == TargetTeam;
		L.m_TeamJoin = TargetInTeam && LocalTeam != TargetTeam;
		L.m_TeamInvite = LocalInTeam && TargetTeam != LocalTeam;
		L.m_TeamKick = !LocalIsTarget && LocalInTeam && TargetTeam == LocalTeam;
		L.m_TeamLock = LocalInTeam && LocalTeam == TargetTeam;
		L.m_NumTeam = (int)L.m_TeamExit + (int)L.m_TeamJoin + (int)L.m_TeamInvite + (int)L.m_TeamKick + (int)L.m_TeamLock;
	}
	return L;
}

int ActionRows(const SLayout &L)
{
	return (L.m_Spectate ? 1 : 0) + (L.m_NumGrid + 1) / 2 + (L.m_Voice ? 1 : 0);
}

float RowsHeight(int Rows)
{
	return Rows <= 0 ? 0.0f : Rows * BTN_H + (Rows - 1) * BTN_GAP;
}

float LayoutHeight(const SLayout &L)
{
	float Height = PAD + HEADER_H + GAP;
	if(!L.m_Local)
		Height += ICON_H + GAP;
	Height += 1.0f + GAP; // divider
	Height += RowsHeight(ActionRows(L));
	if(L.m_NumTeam > 0)
		Height += GAP + LABEL_H + RowsHeight((L.m_NumTeam + 1) / 2);
	Height += PAD;
	return Height;
}

enum
{
	STYLE_NORMAL = 0,
	STYLE_ACCENT,
	STYLE_DANGER,
	STYLE_TEAM,
};

struct SCell
{
	CButtonContainer *m_pButton;
	const char *m_pLabel;
	int m_Style;
	std::function<void()> m_OnClick;
};
} // namespace

float CScoreboard::NeonPlayerPopupHeight() const
{
	const SLayout L = MakeLayout(GameClient(), m_ScoreboardPopupContext.m_IsLocal, m_ScoreboardPopupContext.m_IsSpectating, m_ScoreboardPopupContext.m_ClientId);
	return LayoutHeight(L) + POPUP_INSET * 2.0f;
}

void CScoreboard::OpenPlayerPopup()
{
	m_ScoreboardPopupContext.m_VoiceVolumePreview = -1;
	m_ScoreboardPopupContext.m_VoiceVolumeDirty = false;
	if(g_Config.m_CcStyle)
	{
		// the popup draws nothing itself, the card is drawn in RenderNeon
		SPopupMenuProperties Props;
		Props.m_BorderColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		Props.m_BackgroundColor = ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f);
		Ui()->DoPopupMenu(&m_ScoreboardPopupContext, Ui()->MouseX(), Ui()->MouseY(), 172.0f + POPUP_INSET * 2.0f,
			NeonPlayerPopupHeight(), &m_ScoreboardPopupContext, CScoreboardPopupContext::RenderNeon, Props);
	}
	else
	{
		Ui()->DoPopupMenu(&m_ScoreboardPopupContext, Ui()->MouseX(), Ui()->MouseY(), 150.0f,
			BestClientScoreboardPlayerPopupHeight(GameClient(), m_ScoreboardPopupContext.m_IsLocal, m_ScoreboardPopupContext.m_IsSpectating, m_ScoreboardPopupContext.m_ClientId),
			&m_ScoreboardPopupContext, CScoreboardPopupContext::Render);
	}
}

CUi::EPopupMenuFunctionResult CScoreboard::CScoreboardPopupContext::RenderNeon(void *pContext, CUIRect View, bool Active)
{
	CScoreboardPopupContext *pPopupContext = static_cast<CScoreboardPopupContext *>(pContext);
	CScoreboard *pScoreboard = pPopupContext->m_pScoreboard;
	CUi *pUi = pScoreboard->Ui();
	ITextRender *pTextRender = pScoreboard->TextRender();

	CGameClient::CClientData &Client = pScoreboard->GameClient()->m_aClients[pPopupContext->m_ClientId];
	if(!Client.m_Active)
		return CUi::POPUP_CLOSE_CURRENT;

	const SLayout L = MakeLayout(pScoreboard->GameClient(), pPopupContext->m_IsLocal, pPopupContext->m_IsSpectating, pPopupContext->m_ClientId);

	// the card
	CUIRect Card = View;
	Card.x -= POPUP_INSET;
	Card.y -= POPUP_INSET;
	Card.w += POPUP_INSET * 2.0f;
	Card.h += POPUP_INSET * 2.0f;
	CCNeon::Glow(Card, 8.0f, CCNeon::NEON, 0.6f, IGraphics::CORNER_ALL, 1.25f, 4);
	CCNeon::DrawFramed(Card, CCNeon::PANEL, ColorRGBA(CCNeon::ACCENT_TEXT.r, CCNeon::ACCENT_TEXT.g, CCNeon::ACCENT_TEXT.b, 0.30f), 8.0f);
	Card.Margin(PAD, &View);

	const ColorRGBA TextColor = CCNeon::TEXT;
	auto ResetText = [&]() { pTextRender->TextColor(pTextRender->DefaultTextColor()); };

	// header: the tee and the name
	{
		CUIRect Header, TeeArea, Names, Line;
		View.HSplitTop(HEADER_H, &Header, &View);
		View.HSplitTop(GAP, nullptr, &View);
		Header.VSplitLeft(HEADER_H, &TeeArea, &Names);
		Names.VSplitLeft(6.0f, nullptr, &Names);

		CCNeon::DrawFramed(TeeArea, CCNeon::INPUT, CCNeon::LINE, 6.0f);
		CTeeRenderInfo TeeInfo = Client.m_RenderInfo;
		TeeInfo.m_Size = HEADER_H - 6.0f;
		const vec2 TeePos(TeeArea.x + TeeArea.w / 2.0f, TeeArea.y + TeeArea.h / 2.0f + 1.5f);
		pScoreboard->RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeePos);

		const bool HasClan = Client.m_aClan[0] != '\0';
		if(HasClan)
		{
			Names.HSplitTop(Names.h * 0.58f, &Line, &Names);
		}
		else
		{
			Line = Names;
		}
		pTextRender->TextColor(Client.m_Friend ? CCNeon::ACCENT_TEXT : TextColor);
		pUi->DoLabel(&Line, Client.m_aName, 12.5f, HasClan ? TEXTALIGN_BL : TEXTALIGN_ML);
		if(HasClan)
		{
			pTextRender->TextColor(CCNeon::TEXT_DIM);
			pUi->DoLabel(&Names, Client.m_aClan, 9.5f, TEXTALIGN_TL);
		}
		ResetText();
	}

	// the round actions: friend, mute chat, mute emoticons, teammate
	if(!L.m_Local)
	{
		CUIRect Row;
		View.HSplitTop(ICON_H, &Row, &View);
		View.HSplitTop(GAP, nullptr, &View);

		const CWarDataCache &WarData = pScoreboard->GameClient()->m_WarList.GetWarData(Client.ClientId());
		const bool IsTeammate = WarData.m_WarGroupMatches.size() > 2 && WarData.m_WarGroupMatches[2];

		struct SIcon
		{
			CButtonContainer *m_pButton;
			const char *m_pIcon;
			bool m_On;
			const char *m_pTip;
		};
		const bool FriendHot = pUi->HotItem() == &pPopupContext->m_FriendAction;
		const SIcon aIcons[4] = {
			{&pPopupContext->m_FriendAction, FriendHot && Client.m_Friend ? FontIcon::HEART_CRACK : FontIcon::HEART, Client.m_Friend, Client.m_Friend ? Localize("Remove friend") : Localize("Add friend")},
			{&pPopupContext->m_MuteAction, FontIcon::BAN, Client.m_ChatIgnore != 0, Client.m_ChatIgnore ? Localize("Unmute") : Localize("Mute")},
			{&pPopupContext->m_EmoticonAction, Client.m_EmoticonIgnore ? FontIcon::COMMENT_SLASH : FontIcon::COMMENT, Client.m_EmoticonIgnore != 0, Client.m_EmoticonIgnore ? Localize("Unmute emoticons") : Localize("Mute emoticons")},
			{&pPopupContext->m_TeamAction, FontIcon::ICON_USERS, IsTeammate, IsTeammate ? Localize("Remove from teammates") : Localize("Add to teammates")},
		};
		const float IconW = (Row.w - ICON_GAP * 3.0f) / 4.0f;
		for(int i = 0; i < 4; i++)
		{
			CUIRect Icon;
			Row.VSplitLeft(IconW, &Icon, &Row);
			Row.VSplitLeft(ICON_GAP, nullptr, &Row);
			const SIcon &Item = aIcons[i];

			const bool Clicked = pUi->DoButtonLogic(Item.m_pButton, 0, &Icon, BUTTONFLAG_LEFT) != 0;
			const bool Hot = pUi->HotItem() == Item.m_pButton;
			const bool Pressed = pUi->CheckActiveItem(Item.m_pButton);
			if(Item.m_On)
				CCNeon::DrawFramed(Icon, ColorRGBA(CCNeon::ACCENT.r, CCNeon::ACCENT.g, CCNeon::ACCENT.b, Hot ? 0.45f : 0.32f), ColorRGBA(CCNeon::NEON.r, CCNeon::NEON.g, CCNeon::NEON.b, 0.5f), CCNeon::Scaled(5.0f, Icon));
			else
				CCNeon::DrawSecondary(Icon, Hot, Pressed, IGraphics::CORNER_ALL, 5.0f);

			pTextRender->SetFontPreset(EFontPreset::ICON_FONT);
			pTextRender->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
			pTextRender->TextColor(Item.m_On ? CCNeon::ACCENT_TEXT : (Hot ? TextColor : CCNeon::TEXT_SUB));
			CUIRect IconLabel;
			Icon.HMargin(2.0f, &IconLabel);
			pUi->DoLabel(&IconLabel, Item.m_pIcon, IconLabel.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
			pTextRender->SetRenderFlags(0);
			pTextRender->SetFontPreset(EFontPreset::DEFAULT_FONT);
			ResetText();

			pScoreboard->GameClient()->m_Tooltips.DoToolTip(Item.m_pButton, &Icon, Item.m_pTip);

			if(Clicked)
			{
				if(i == 0)
				{
					if(Client.m_Friend)
						pScoreboard->GameClient()->Friends()->RemoveFriend(Client.m_aName, Client.m_aClan);
					else
						pScoreboard->GameClient()->Friends()->AddFriend(Client.m_aName, Client.m_aClan);
				}
				else if(i == 1)
					Client.m_ChatIgnore ^= 1;
				else if(i == 2)
					Client.m_EmoticonIgnore ^= 1;
				else if(IsTeammate)
					pScoreboard->GameClient()->m_WarList.RemoveWarEntryInGame(2, Client.m_aName, false);
				else
					pScoreboard->GameClient()->m_WarList.AddWarEntryInGame(2, Client.m_aName, "", false);
			}
		}
	}

	// divider
	{
		CUIRect Divider;
		View.HSplitTop(1.0f, &Divider, &View);
		View.HSplitTop(GAP, nullptr, &View);
		Divider.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f), IGraphics::CORNER_NONE, 0.0f);
	}

	auto SayNamed = [&](const char *pCommand) {
		char aEscapedCommand[2 * MAX_NAME_LENGTH + 64];
		str_format(aEscapedCommand, sizeof(aEscapedCommand), "say %s \"", pCommand);
		char *pDst = aEscapedCommand + str_length(aEscapedCommand);
		str_escape(&pDst, Client.m_aName, aEscapedCommand + sizeof(aEscapedCommand));
		str_append(aEscapedCommand, "\"");
		pScoreboard->Console()->ExecuteLine(aEscapedCommand, IConsole::CLIENT_ID_UNSPECIFIED);
	};

	auto DoButton = [&](const SCell &Cell, CUIRect Rect, bool Selected) {
		const bool Clicked = pUi->DoButtonLogic(Cell.m_pButton, 0, &Rect, BUTTONFLAG_LEFT) != 0;
		const bool Hot = pUi->HotItem() == Cell.m_pButton;
		const bool Pressed = pUi->CheckActiveItem(Cell.m_pButton);
		ColorRGBA LabelColor = TextColor;
		if(Cell.m_Style == STYLE_TEAM || Selected)
		{
			const float Fill = Selected ? 0.34f : (Hot ? 0.30f : 0.20f);
			CCNeon::DrawFramed(Rect, ColorRGBA(CCNeon::ACCENT.r, CCNeon::ACCENT.g, CCNeon::ACCENT.b, Fill), ColorRGBA(CCNeon::NEON.r, CCNeon::NEON.g, CCNeon::NEON.b, Hot || Selected ? 0.55f : 0.35f), CCNeon::Scaled(4.0f, Rect));
			LabelColor = CCNeon::ACCENT_TEXT;
		}
		else
		{
			CCNeon::DrawSecondary(Rect, Hot, Pressed, IGraphics::CORNER_ALL, 4.0f);
			if(Cell.m_Style == STYLE_DANGER)
				LabelColor = CCNeon::ACCENT_TEXT;
			else if(!Hot)
				LabelColor = ColorRGBA(TextColor.r, TextColor.g, TextColor.b, 0.92f);
		}
		pTextRender->TextColor(LabelColor);
		pUi->DoLabel(&Rect, Cell.m_pLabel, FONT, TEXTALIGN_MC);
		ResetText();
		if(Clicked && Cell.m_OnClick)
			Cell.m_OnClick();
	};

	int RowsLeft = ActionRows(L);
	auto NextRow = [&](CUIRect *pRow) {
		View.HSplitTop(BTN_H, pRow, &View);
		if(--RowsLeft > 0)
			View.HSplitTop(BTN_GAP, nullptr, &View);
	};
	auto DoGrid = [&](const std::vector<SCell> &vCells) {
		for(size_t i = 0; i < vCells.size(); i += 2)
		{
			CUIRect Row;
			NextRow(&Row);
			if(i + 1 < vCells.size())
			{
				CUIRect Left, Right;
				Row.VSplitMid(&Left, &Right, BTN_GAP);
				DoButton(vCells[i], Left, false);
				DoButton(vCells[i + 1], Right, false);
			}
			else
			{
				DoButton(vCells[i], Row, false);
			}
		}
	};

	// spectate
	if(L.m_Spectate)
	{
		const bool IsSpectating = pScoreboard->GameClient()->m_Snap.m_SpecInfo.m_Active && pScoreboard->GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == pPopupContext->m_ClientId;
		CUIRect Row;
		NextRow(&Row);
		const SCell Cell = {&pPopupContext->m_SpectateButton, Localize("Spectate"), STYLE_NORMAL, [&]() {
					    if(IsSpectating)
					    {
						    pScoreboard->GameClient()->m_Spectator.Spectate(SPEC_FREEVIEW);
						    pScoreboard->Console()->ExecuteLine("say /spec", IConsole::CLIENT_ID_UNSPECIFIED);
					    }
					    else if(pScoreboard->GameClient()->m_Snap.m_SpecInfo.m_Active)
					    {
						    pScoreboard->GameClient()->m_Spectator.Spectate(pPopupContext->m_ClientId);
					    }
					    else
					    {
						    // escape the name
						    char aEscapedCommand[2 * MAX_NAME_LENGTH + 32];
						    str_copy(aEscapedCommand, "say /spec \"");
						    char *pDst = aEscapedCommand + str_length(aEscapedCommand);
						    str_escape(&pDst, Client.m_aName, aEscapedCommand + sizeof(aEscapedCommand));
						    str_append(aEscapedCommand, "\"");
						    pScoreboard->Console()->ExecuteLine(aEscapedCommand, IConsole::CLIENT_ID_UNSPECIFIED);
					    }
				    }};
		DoButton(Cell, Row, IsSpectating);
	}

	// the other actions in two columns
	std::vector<SCell> vCells;
	vCells.push_back({&pPopupContext->m_ProfileButton, Localize("Profile"), STYLE_NORMAL, [&]() {
				  const CServerInfo &ServerInfo = pScoreboard->Client()->ServerInfo();
				  char aEncodedName[256];
				  EscapeUrl(aEncodedName, sizeof(aEncodedName), Client.m_aName);
				  char aLink[512];
				  if(str_comp(ServerInfo.m_aCommunityId, "kog") == 0)
					  str_format(aLink, sizeof(aLink), "https://kog.tw/#p=players&player=%s", aEncodedName);
				  else if(str_comp(ServerInfo.m_aCommunityId, "unique") == 0)
					  str_format(aLink, sizeof(aLink), "https://uniqueclan.net/ranks/player/%s", aEncodedName);
				  else
					  str_format(aLink, sizeof(aLink), "https://ddnet.org/players/%s", aEncodedName);
				  pScoreboard->Client()->ViewLink(aLink);
			  }});
	vCells.push_back({&pPopupContext->m_FindHoursButton, Localize("Find Hours"), STYLE_NORMAL, [&]() { pScoreboard->GameClient()->m_BestClient.FetchFindHours(Client.m_aName); }});
	if(!L.m_Local)
	{
		vCells.push_back({&pPopupContext->m_CopySkinButton, Localize("Copy Skin"), STYLE_NORMAL, [&]() { pScoreboard->GameClient()->m_BestClient.CopyPlayerSkin(pPopupContext->m_ClientId); }});
		vCells.push_back({&pPopupContext->m_WhisperButton, Localize("Whisper"), STYLE_NORMAL, [&]() {
					  char aWhisperBuf[256];
					  str_copy(aWhisperBuf, "chat all \"/whisper ", sizeof(aWhisperBuf));
					  char *pDst = aWhisperBuf + str_length(aWhisperBuf);
					  str_escape(&pDst, Client.m_aName, aWhisperBuf + sizeof(aWhisperBuf) - 2);
					  str_append(aWhisperBuf, " \"");
					  pScoreboard->Console()->ExecuteLine(aWhisperBuf, IConsole::CLIENT_ID_UNSPECIFIED);
				  }});
	}
	vCells.push_back({&pPopupContext->m_ClipNameButton, Localize("Clip Name"), STYLE_NORMAL, [&]() { pScoreboard->Input()->SetClipboardText(Client.m_aName); }});
	if(!L.m_Local)
	{
		vCells.push_back({&pPopupContext->m_SwapButton, Localize("/Swap"), STYLE_NORMAL, [&]() { SayNamed("/swap"); }});
		vCells.push_back({&pPopupContext->m_VoteKickButton, Localize("Vote Kick"), STYLE_DANGER, [&]() { pScoreboard->GameClient()->m_Voting.CallvoteKick(pPopupContext->m_ClientId, ""); }});
	}
	const bool VoiceMuted = L.m_Voice && pScoreboard->GameClient()->m_VoiceChat.IsNameMuted(Client.m_aName);
	if(L.m_Voice)
	{
		vCells.push_back({&pPopupContext->m_VoiceMuteButton, VoiceMuted ? Localize("Voice unmute") : Localize("Voice mute"), STYLE_NORMAL, [&]() {
					  char aCmd[MAX_NAME_LENGTH + 32];
					  str_format(aCmd, sizeof(aCmd), !VoiceMuted ? "!vmute \"%s\"" : "!vunmute \"%s\"", Client.m_aName);
					  pScoreboard->GameClient()->m_VoiceChat.TryHandleChatCommand(aCmd);
				  }});
	}
	DoGrid(vCells);

	// voice volume of this player
	if(L.m_Voice)
	{
		CUIRect Row, VolumeLabel, VolumeSlider;
		NextRow(&Row);
		const int ConfigVoiceVolume = pScoreboard->GameClient()->m_VoiceChat.GetNameVolumePercent(Client.m_aName);
		if(pPopupContext->m_VoiceVolumePreview < 1 || pPopupContext->m_VoiceVolumePreview > 100)
			pPopupContext->m_VoiceVolumePreview = ConfigVoiceVolume;
		if(!pUi->CheckActiveItem(&pPopupContext->m_VoiceVolumeSlider) && !pPopupContext->m_VoiceVolumeDirty)
			pPopupContext->m_VoiceVolumePreview = ConfigVoiceVolume;

		Row.VSplitLeft(34.0f, &VolumeLabel, &VolumeSlider);
		char aVoiceVolume[16];
		str_format(aVoiceVolume, sizeof(aVoiceVolume), "%d%%", std::clamp(pPopupContext->m_VoiceVolumePreview, 1, 100));
		pTextRender->TextColor(CCNeon::TEXT_SUB);
		pUi->DoLabel(&VolumeLabel, aVoiceVolume, FONT, TEXTALIGN_ML);
		ResetText();
		const float CurrentRel = std::clamp(pPopupContext->m_VoiceVolumePreview / 100.0f, 0.01f, 1.0f);
		const float NewRel = pUi->DoScrollbarH(&pPopupContext->m_VoiceVolumeSlider, &VolumeSlider, CurrentRel);
		const int NewVolume = std::clamp((int)std::lround(NewRel * 100.0f), 1, 100);
		if(NewVolume != pPopupContext->m_VoiceVolumePreview)
		{
			pPopupContext->m_VoiceVolumePreview = NewVolume;
			pPopupContext->m_VoiceVolumeDirty = true;
		}
		if(pPopupContext->m_VoiceVolumeDirty && !pUi->CheckActiveItem(&pPopupContext->m_VoiceVolumeSlider))
		{
			char aCmd[MAX_NAME_LENGTH + 48];
			str_format(aCmd, sizeof(aCmd), "!volume \"%s\" %d", Client.m_aName, std::clamp(pPopupContext->m_VoiceVolumePreview, 1, 100));
			pScoreboard->GameClient()->m_VoiceChat.TryHandleChatCommand(aCmd);
			pPopupContext->m_VoiceVolumeDirty = false;
		}
	}

	// team actions
	if(L.m_NumTeam > 0)
	{
		View.HSplitTop(GAP, nullptr, &View);
		CUIRect Label;
		View.HSplitTop(LABEL_H, &Label, &View);
		pTextRender->TextColor(CCNeon::TEXT_DIM);
		pUi->DoLabel(&Label, Localize("Team"), 9.0f, TEXTALIGN_ML);
		ResetText();

		const int LocalId = pScoreboard->GameClient()->m_aLocalIds[g_Config.m_ClDummy];
		const int LocalTeam = LocalId >= 0 ? pScoreboard->GameClient()->m_Teams.Team(LocalId) : TEAM_FLOCK;
		const int TargetTeam = pScoreboard->GameClient()->m_Teams.Team(pPopupContext->m_ClientId);

		std::vector<SCell> vTeam;
		if(L.m_TeamExit)
			vTeam.push_back({&pPopupContext->m_TeamExitButton, Localize("Exit"), STYLE_TEAM, [&]() { pScoreboard->Console()->ExecuteLine("say /team 0", IConsole::CLIENT_ID_UNSPECIFIED); }});
		if(L.m_TeamJoin)
			vTeam.push_back({&pPopupContext->m_TeamJoinButton, Localize("Join"), STYLE_TEAM, [&, TargetTeam]() {
						 char aCmd[64];
						 str_format(aCmd, sizeof(aCmd), "say /team %d", TargetTeam);
						 pScoreboard->Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
					 }});
		if(L.m_TeamInvite)
			vTeam.push_back({&pPopupContext->m_TeamInviteButton, Localize("Invite"), STYLE_TEAM, [&]() { SayNamed("/invite"); }});
		if(L.m_TeamKick)
			vTeam.push_back({&pPopupContext->m_TeamKickButton, Localize("Kick"), STYLE_TEAM, [&]() { pScoreboard->GameClient()->m_Voting.CallvoteKick(pPopupContext->m_ClientId, ""); }});
		if(L.m_TeamLock)
			vTeam.push_back({&pPopupContext->m_TeamLockButton, Localize("Lock"), STYLE_TEAM, [&]() { pScoreboard->Console()->ExecuteLine("say /lock", IConsole::CLIENT_ID_UNSPECIFIED); }});
		(void)LocalTeam;

		RowsLeft = (int)(vTeam.size() + 1) / 2;
		DoGrid(vTeam);
	}

	return CUi::POPUP_KEEP_OPEN;
}
