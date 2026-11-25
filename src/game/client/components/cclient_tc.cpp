/* Cr1stal Client: small features ported from TClient (https://github.com/sjrc6/TaterClient-ddnet). Based on DDNet, see licence.txt. */
#include "cclient_tc.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/kernel.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <cmath>
#include <cstdlib>
#include <vector>

// ---------------------------------------------------------------- rainbow

void CCRainbow::OnRender()
{
	if(!g_Config.m_CcRainbowTee && !g_Config.m_CcRainbowWeapon && !g_Config.m_CcRainbowHook)
		return;

	m_Time += Client()->RenderFrameTime() * (g_Config.m_CcRainbowSpeed / 100.0f);
	const float Phase = std::fmod(m_Time, 1.0f);
	switch(g_Config.m_CcRainbowMode)
	{
	case 2: // pulse
		m_RainbowColor = color_cast<ColorRGBA>(ColorHSLA(std::fmod(std::floor(m_Time) * 0.1f, 1.0f), 1.0f, 0.5f + std::fabs(Phase - 0.5f)));
		break;
	case 3: // darkness
		m_RainbowColor = ColorRGBA(0.0f, 0.0f, 0.0f, 1.0f);
		break;
	case 4: // random, smoothly from one color to the next
		if(m_RandomTo.a == 0.0f)
			m_RandomTo = ColorHSLA((float)rand() / (float)RAND_MAX, 1.0f, (float)rand() / (float)RAND_MAX, 1.0f);
		if(m_Time - m_LastSwap > 1.0f)
		{
			m_LastSwap = m_Time;
			m_RandomFrom = m_RandomTo;
			m_RandomTo = ColorHSLA((float)rand() / (float)RAND_MAX, 1.0f, (float)rand() / (float)RAND_MAX, 1.0f);
		}
		m_RainbowColor = color_cast<ColorRGBA>(ColorHSLA(
			m_RandomFrom.h + (m_RandomTo.h - m_RandomFrom.h) * Phase,
			m_RandomFrom.s + (m_RandomTo.s - m_RandomFrom.s) * Phase,
			m_RandomFrom.l + (m_RandomTo.l - m_RandomFrom.l) * Phase, 1.0f));
		break;
	default: // rainbow
		m_RainbowColor = color_cast<ColorRGBA>(ColorHSLA(Phase, 1.0f, 0.5f));
		break;
	}
}

// ---------------------------------------------------------------- automation

static bool LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHL = str_utf8_find_nocase(pLine, pName);
	if(pHL)
	{
		const int Length = str_length(pName);
		if(Length > 0 && (pLine == pHL || pHL[-1] == ' ') && (pHL[Length] == 0 || pHL[Length] == ' ' || pHL[Length] == '.' || pHL[Length] == '!' || pHL[Length] == ',' || pHL[Length] == '?' || pHL[Length] == ':'))
			return true;
	}
	return false;
}

bool CCAutomation::SendNonDuplicateMessage(int Team, const char *pLine)
{
	if(str_comp(pLine, m_aPreviousOwnMessage) != 0)
	{
		GameClient()->m_Chat.SendChat(Team, pLine);
		return true;
	}
	str_copy(m_aPreviousOwnMessage, pLine);
	return false;
}

void CCAutomation::OnMessage(int MsgType, void *pRawMsg)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		const CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		const int ClientId = pMsg->m_ClientId;
		if(ClientId < 0 || ClientId >= MAX_CLIENTS)
			return;
		if(ClientId == GameClient()->m_Snap.m_LocalClientId)
			str_copy(m_aPreviousOwnMessage, pMsg->m_pMessage);

		if(!g_Config.m_CcAutoReplyMuted && !g_Config.m_CcAutoReplyMinimized)
			return;

		// only messages that mention you or whisper you
		bool PingMessage = pMsg->m_Team == TEAM_WHISPER_RECV;
		const bool ValidIds = !(GameClient()->m_aLocalIds[0] < 0 || (Client()->DummyConnected() && GameClient()->m_aLocalIds[1] < 0));
		if(ValidIds && ClientId != GameClient()->m_aLocalIds[0] && (!Client()->DummyConnected() || ClientId != GameClient()->m_aLocalIds[1]))
		{
			PingMessage |= LineShouldHighlight(pMsg->m_pMessage, GameClient()->m_aClients[GameClient()->m_aLocalIds[0]].m_aName);
			PingMessage |= Client()->DummyConnected() && LineShouldHighlight(pMsg->m_pMessage, GameClient()->m_aClients[GameClient()->m_aLocalIds[1]].m_aName);
		}
		if(!PingMessage)
			return;

		const char *pName = GameClient()->m_aClients[ClientId].m_aName;
		const bool Whisper = pMsg->m_Team == TEAM_WHISPER_RECV || GameClient()->m_Chat.ServerCommandExists("w");
		const bool Muted = GameClient()->m_aClients[ClientId].m_Foe || GameClient()->m_aClients[ClientId].m_ChatIgnore;
		const char *pReply = nullptr;
		if(g_Config.m_CcAutoReplyMuted && Muted)
			pReply = g_Config.m_CcAutoReplyMutedMessage;
		else if(g_Config.m_CcAutoReplyMinimized && m_pEngineGraphics && !m_pEngineGraphics->WindowActive())
			pReply = g_Config.m_CcAutoReplyMinimizedMessage;
		if(!pReply || pReply[0] == '\0')
			return;
		char aBuf[256];
		if(Whisper)
			str_format(aBuf, sizeof(aBuf), "/w %s %s", pName, pReply);
		else
			str_format(aBuf, sizeof(aBuf), "%s: %s", pName, pReply);
		SendNonDuplicateMessage(0, aBuf);
		return;
	}

	if(MsgType == NETMSGTYPE_SV_VOTESET && g_Config.m_CcAutoVoteWhenFar)
	{
		const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
		const bool Afk = LocalId >= 0 && GameClient()->m_aClients[LocalId].m_Afk;
		const CNetMsg_Sv_VoteSet *pMsg = (CNetMsg_Sv_VoteSet *)pRawMsg;
		if(!pMsg->m_Timeout || Afk)
			return;

		const char *pDescription = pMsg->m_pDescription;
		const bool KickVote = str_startswith(pDescription, "Kick ") != nullptr;
		const bool SpecVote = str_startswith(pDescription, "Pause ") != nullptr;
		const bool SettingVote = !KickVote && !SpecVote;
		const bool RandomMapVote = SettingVote && str_find_nocase(pDescription, "random");
		const bool MapCoolDown = SettingVote && (str_find_nocase(pDescription, "change map") || str_find_nocase(pDescription, "no not change map"));
		const bool CategoryVote = SettingVote && (str_find_nocase(pDescription, "\xe2\x98\x90") || str_find_nocase(pDescription, "\xe2\x98\x92"));
		const bool FunVote = SettingVote && str_find_nocase(pDescription, "funvote");
		const bool MapVote = SettingVote && !RandomMapVote && !MapCoolDown && !CategoryVote && !FunVote &&
				     (str_find_nocase(pDescription, "Map:") || str_find_nocase(pDescription, "\xe2\x98\x85") || str_find_nocase(pDescription, "\xe2\x9c\xb0"));
		if(!MapVote && !RandomMapVote)
			return;

		int RaceTime = 0;
		if(GameClient()->m_Snap.m_pGameInfoObj && GameClient()->m_Snap.m_pGameInfoObj->m_GameStateFlags & GAMESTATEFLAG_RACETIME)
			RaceTime = (Client()->GameTick(g_Config.m_ClDummy) + GameClient()->m_Snap.m_pGameInfoObj->m_WarmupTimer) / Client()->GameTickSpeed();
		if(RaceTime / 60 < g_Config.m_CcAutoVoteWhenFarTime)
			return;

		// who called the vote: friends, your team and you yourself are not voted against
		int CallerId = -1;
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(!GameClient()->m_aStats[i].IsActive())
				continue;
			char aBuf[MAX_NAME_LENGTH + 4];
			str_format(aBuf, sizeof(aBuf), "'%s'", GameClient()->m_aClients[i].m_aName);
			if(str_find_nocase(pDescription, aBuf) != nullptr)
			{
				CallerId = i;
				break;
			}
		}
		if(CallerId < 0)
			return;
		const CGameClient::CClientData &Caller = GameClient()->m_aClients[CallerId];
		const bool SameTeam = GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) == Caller.m_Team && Caller.m_Team != 0;
		if(Caller.m_Friend || SameTeam || CallerId == LocalId)
			return;
		GameClient()->m_Voting.Vote(-1);
		if(g_Config.m_CcAutoVoteWhenFarMessage[0] != '\0')
			SendNonDuplicateMessage(0, g_Config.m_CcAutoVoteWhenFarMessage);
	}
}

void CCAutomation::OnStateChange(int NewState, int OldState)
{
	if(NewState == IClient::STATE_CONNECTING && OldState != IClient::STATE_CONNECTING && g_Config.m_CcExecuteOnConnect[0] != '\0')
		Console()->ExecuteLine(g_Config.m_CcExecuteOnConnect, IConsole::CLIENT_ID_UNSPECIFIED);
	if(NewState != IClient::STATE_ONLINE)
	{
		for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
		{
			m_aSnapshotsSinceJoin[Dummy] = 0;
			m_aJoinDone[Dummy] = false;
		}
	}
}

void CCAutomation::OnNewSnapshot()
{
	// cc_execute_on_join, a few ticks after joining
	if(Client()->State() != IClient::STATE_ONLINE || m_aJoinDone[0])
		return;
	m_aSnapshotsSinceJoin[0]++;
	if(m_aSnapshotsSinceJoin[0] > g_Config.m_CcExecuteOnJoinDelay)
	{
		m_aJoinDone[0] = true;
		if(g_Config.m_CcExecuteOnJoin[0] != '\0')
			Console()->ExecuteLine(g_Config.m_CcExecuteOnJoin, IConsole::CLIENT_ID_UNSPECIFIED);
	}
}

void CCAutomation::OnInit()
{
	m_pEngineGraphics = Kernel()->RequestInterface<IEngineGraphics>();
}

void CCAutomation::OnRender()
{
	DoFinishCheck();
}

void CCAutomation::DoFinishCheck()
{
	if(Client()->State() != IClient::STATE_ONLINE || !g_Config.m_CcChangeNameNearFinish)
		return;
	m_FinishCheckTimeout -= Client()->RenderFrameTime();
	if(m_FinishCheckTimeout > 0.0f)
		return;
	m_FinishCheckTimeout = 1.0f;

	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0)
		return;
	const CGameClient::CClientData &Player = GameClient()->m_aClients[LocalId];
	if(!Player.m_Active || str_comp(Player.m_aName, g_Config.m_CcFinishName) == 0)
		return;

	// a finish tile in a circle of 10 tiles around you
	const CCollision *pCollision = Collision();
	const vec2 Pos = Player.m_RenderPos;
	const int Radius = 10;
	bool Near = false;
	for(int i = 0; i <= Radius * 2 && !Near; ++i)
	{
		const float h = std::ceil(std::pow(std::sin((float)i * pi / 2.0f / (float)Radius), 0.5f) * pi / 2.0f * (float)Radius);
		const vec2 Pos1 = vec2(Pos.x + (float)(i - Radius) * 32.0f, Pos.y - h);
		const vec2 Pos2 = vec2(Pos.x + (float)(i - Radius) * 32.0f, Pos.y + h);
		std::vector<int> vIndices = pCollision->GetMapIndices(Pos1, Pos2);
		if(vIndices.empty())
			vIndices.push_back(pCollision->GetPureMapIndex(Pos1));
		for(const int Index : vIndices)
		{
			if(pCollision->GetTileIndex(Index) == TILE_FINISH || pCollision->GetFrontTileIndex(Index) == TILE_FINISH)
			{
				Near = true;
				break;
			}
		}
	}
	if(!Near)
		return;

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), Localize("Changing name to %s near finish"), g_Config.m_CcFinishName);
	GameClient()->Echo(aBuf);

	CNetMsg_Cl_ChangeInfo Msg;
	Msg.m_pName = g_Config.m_CcFinishName;
	Msg.m_pClan = Dummy == 0 ? g_Config.m_PlayerClan : g_Config.m_ClDummyClan;
	Msg.m_Country = Dummy == 0 ? g_Config.m_PlayerCountry : g_Config.m_ClDummyCountry;
	Msg.m_pSkin = Dummy == 0 ? g_Config.m_ClPlayerSkin : g_Config.m_ClDummySkin;
	Msg.m_UseCustomColor = Dummy == 0 ? g_Config.m_ClPlayerUseCustomColor : g_Config.m_ClDummyUseCustomColor;
	Msg.m_ColorBody = Dummy == 0 ? g_Config.m_ClPlayerColorBody : g_Config.m_ClDummyColorBody;
	Msg.m_ColorFeet = Dummy == 0 ? g_Config.m_ClPlayerColorFeet : g_Config.m_ClDummyColorFeet;
	CMsgPacker Packer(&Msg);
	Msg.Pack(&Packer);
	Client()->SendMsg(Dummy, &Packer, MSGFLAG_VITAL);
	GameClient()->SetCheckInfo(Dummy, Client()->GameTickSpeed()); // put the real name back after a second
}

// ---------------------------------------------------------------- pet

void CCPet::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;
	if(g_Config.m_CcPetShow <= 0)
		return;

	const int PlayerId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(PlayerId < 0)
		return;
	const auto &Player = GameClient()->m_aClients[PlayerId];
	const float Delta = Client()->RenderFrameTime();
	const float Now = (float)Client()->GameTick(g_Config.m_ClDummy) / (float)Client()->GameTickSpeed();

	const float Scale = (float)g_Config.m_CcPetSize / 100.0f;

	if(Player.m_Active && Player.m_Team != TEAM_SPECTATORS)
	{
		m_Target = Player.m_RenderPos;
		m_Target.x += (64.0f + Scale * 32.0f) * (m_Position.x > m_Target.x ? 1 : -1);
		m_Target.y -= 100.0f + Scale * 32.0f;
		m_Target.y += std::sin(Now / 2.0f) * 8.0f;
		if(m_Alpha == 0.0f)
		{
			m_Position = m_Target;
			m_Velocity = vec2(0.0f, 0.0f);
			m_Dir = random_direction();
		}
		if(m_Alpha < 1.0f)
		{
			m_Alpha += Delta;
			if(m_Alpha >= 1.0f)
				m_Alpha = 1.0f;
		}
		m_Position += m_Velocity * Delta;

		const vec2 DeltaPosition = m_Target - m_Position;
		const float DeltaLength = length(DeltaPosition);
		if(DeltaLength > 512.0f)
			m_Alpha = 0.01f;

		static const float k = 50.0f;
		const vec2 DeltaDamped = (m_Velocity * -2.0f * std::sqrt(k) + DeltaPosition * k) * Delta;
		static const float Friction = 0.01f;
		const vec2 DeltaWizzy = (m_Velocity + DeltaPosition * Delta * 50.0f) * std::pow(Friction, Delta) - m_Velocity;
		m_Velocity += mix(DeltaDamped, DeltaWizzy, std::clamp(DeltaLength / 64.0f, 0.0f, 1.0f));
	}
	else
	{
		if(m_Alpha > 0.0f)
		{
			m_Alpha -= Delta;
			if(m_Alpha <= 0.0f)
				m_Alpha = 0.0f;
		}
	}

	if(m_Alpha <= 0.0f)
		return;

	const auto &Character = Player.m_RenderCur;

	vec2 DirTarget;
	int Emote = 0;
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		DirTarget = GameClient()->m_Camera.m_Center - m_Target;
		if(length(DirTarget) > 1.0f)
			DirTarget = normalize(DirTarget);
	}
	else
	{
		Emote = Character.m_Emote;
		vec2 DirMouse = GameClient()->m_Controls.m_aMousePos[g_Config.m_ClDummy];
		if(length(DirMouse) > 1.0f)
			DirMouse = normalize(DirMouse);
		vec2 DirVel = m_Velocity;
		if(length(DirVel) > 1.0f)
			DirVel = normalize(DirVel);
		DirTarget = mix(DirMouse, DirVel, std::clamp(length(m_Velocity) / 32.0f, 0.0f, 1.0f));
	}
	m_Dir = (DirTarget + m_Dir) / 2.0f; // TODO: stop being lazy

	CTeeRenderInfo TeeRenderInfo;
	TeeRenderInfo.Apply(GameClient()->m_Skins.Find(g_Config.m_CcPetSkin));
	// TeeRenderInfo.ApplyColors(g_Config.m_ClPlayerUseCustomColor, g_Config.m_ClPlayerColorBody, g_Config.m_ClPlayerColorFeet);
	TeeRenderInfo.m_Size = 64.0f * Scale;
	TeeRenderInfo.m_GotAirJump = m_Velocity.y > -10.0f;
	RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, Emote, m_Dir, m_Position, m_Alpha * (float)g_Config.m_CcPetAlpha / 100.0f);
}

void CCPet::OnMapLoad()
{
	m_Alpha = 0.0f;
}
