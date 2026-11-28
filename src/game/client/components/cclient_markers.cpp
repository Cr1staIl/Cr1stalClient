/* Cr1stal Client: markers your team can see ("hook here"). Based on DDNet, see licence.txt. */
#include "cclient_markers.h"

#include "cclient_presence.h"

#include <base/io.h>
#include <base/secure.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/controls.h>
#include <game/client/gameclient.h>
#include <game/localization.h>
#include <game/teamscore.h>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int64_t TTL_SECONDS = 12;
constexpr float SAME_SPOT = 48.0f;

ColorRGBA KindColor(int Kind)
{
	switch(Kind)
	{
	case CCMarkers::KIND_HOOK: return ColorRGBA(0.35f, 0.85f, 1.0f, 1.0f);
	case CCMarkers::KIND_DANGER: return ColorRGBA(1.0f, 0.28f, 0.32f, 1.0f);
	case CCMarkers::KIND_GO: return ColorRGBA(0.45f, 1.0f, 0.55f, 1.0f);
	default: return ColorRGBA(1.0f, 0.82f, 0.30f, 1.0f);
	}
}

const char *KindLabel(int Kind)
{
	switch(Kind)
	{
	case CCMarkers::KIND_HOOK: return Localize("Hook here");
	case CCMarkers::KIND_DANGER: return Localize("Danger");
	case CCMarkers::KIND_GO: return Localize("Go");
	default: return Localize("Here");
	}
}
} // namespace

void CCMarkers::OnConsoleInit()
{
	Console()->Register("cc_marker", "?i[kind]", CFGFLAG_CLIENT, ConMarker, this, "Put a marker for your team where the cursor is: 0 here, 1 hook here, 2 danger, 3 go");
	Console()->Register("cc_marker_clear", "", CFGFLAG_CLIENT, ConMarkerClear, this, "Take your marker away");
}

void CCMarkers::ConMarker(IConsole::IResult *pResult, void *pUserData)
{
	CCMarkers *pThis = static_cast<CCMarkers *>(pUserData);
	const int Kind = pResult->NumArguments() > 0 ? pResult->GetInteger(0) : KIND_HERE;
	pThis->Place(std::clamp(Kind, 0, (int)NUM_KINDS - 1));
}

void CCMarkers::ConMarkerClear(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CCMarkers *>(pUserData)->ClearOwn();
}

bool CCMarkers::ScopePath(char *pBuf, int Size) const
{
	pBuf[0] = '\0';
	if(!g_Config.m_CcSharePresence || !g_Config.m_CcMarkers || Client()->State() != IClient::STATE_ONLINE)
		return false;
	const char *pServer = GameClient()->m_CPresence.ServerHash();
	const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
	if(pServer[0] == '\0' || LocalId < 0 || LocalId >= MAX_CLIENTS)
		return false;
	const int Team = GameClient()->m_Teams.Team(LocalId);
	// the markers are for a team, in no team everyone on the server would get them
	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
		return false;
	str_format(pBuf, Size, "m/%s/%d", pServer, Team);
	return true;
}

void CCMarkers::RemoveOwnRemote()
{
	if(m_aOwnPath[0] != '\0')
		GameClient()->m_CPresence.Send(m_aOwnPath, "DELETE", "null");
	m_aOwnPath[0] = '\0';
	m_OwnRemoteUntil = 0;
}

void CCMarkers::ClearOwn()
{
	m_vMarkers.erase(std::remove_if(m_vMarkers.begin(), m_vMarkers.end(), [](const SMarker &Marker) { return Marker.m_Own; }), m_vMarkers.end());
	RemoveOwnRemote();
}

void CCMarkers::Place(int Kind)
{
	if(Client()->State() != IClient::STATE_ONLINE)
		return;
	const int Dummy = g_Config.m_ClDummy;
	const int LocalId = GameClient()->m_aLocalIds[Dummy];
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return;
	const vec2 Pos = GameClient()->m_Controls.m_aTargetPos[Dummy];

	// the same key on the same spot takes the marker away
	for(const SMarker &Marker : m_vMarkers)
	{
		if(Marker.m_Own && Marker.m_Kind == Kind && distance(Marker.m_Pos, Pos) < SAME_SPOT)
		{
			ClearOwn();
			return;
		}
	}
	ClearOwn();

	if(m_aOwnKey[0] == '\0')
	{
		unsigned Value = 0;
		secure_random_fill(&Value, sizeof(Value));
		str_format(m_aOwnKey, sizeof(m_aOwnKey), "%08x", Value);
	}

	SMarker Marker;
	Marker.m_Id = 1 + secure_rand_below(2000000000);
	Marker.m_Pos = Pos;
	Marker.m_Kind = Kind;
	str_copy(Marker.m_aName, GameClient()->m_aClients[LocalId].m_aName);
	Marker.m_Born = time_get();
	Marker.m_Own = true;
	m_vMarkers.push_back(Marker);
	m_Seen.insert(Marker.m_Id);

	// to the team: only when there is someone who can see it
	char aScope[64];
	bool HasCrystalTeammate = false;
	const int Team = GameClient()->m_Teams.Team(LocalId);
	for(int i = 0; i < MAX_CLIENTS && !HasCrystalTeammate; i++)
		HasCrystalTeammate = i != LocalId && GameClient()->m_aClients[i].m_Active && GameClient()->m_Teams.Team(i) == Team && GameClient()->m_CPresence.IsCrystal(i);
	if(ScopePath(aScope, sizeof(aScope)) && HasCrystalTeammate)
	{
		str_format(m_aOwnPath, sizeof(m_aOwnPath), "%s/%s", aScope, m_aOwnKey);
		char aEscaped[128];
		char aBody[320];
		str_format(aBody, sizeof(aBody), "{\"i\":%d,\"x\":%d,\"y\":%d,\"k\":%d,\"n\":\"%s\",\"t\":{\".sv\":\"timestamp\"}}",
			Marker.m_Id, (int)Pos.x, (int)Pos.y, Kind, EscapeJson(aEscaped, sizeof(aEscaped), Marker.m_aName));
		GameClient()->m_CPresence.Send(m_aOwnPath, "PUT", aBody);
		m_OwnRemoteUntil = time_get() + time_freq() * (TTL_SECONDS + 3);
	}
	else if(!m_HintShown)
	{
		m_HintShown = true;
		GameClient()->Echo(Localize("Only you see this marker: your team sees it when you are in a team with another Cr1stal Client player"));
	}
}

void CCMarkers::Poll(const char *pScope)
{
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%s/%s.json", CCPresence::DatabaseUrl(), pScope);
	m_pPoll = HttpGet(aUrl);
	m_pPoll->Timeout(CTimeout{4000, 10000, 500, 5});
	m_pPoll->MaxResponseSize(64 * 1024);
	m_pPoll->LogProgress(HTTPLOG::NONE);
	Http()->Run(m_pPoll);
}

void CCMarkers::ReadPoll()
{
	if(!m_pPoll || !m_pPoll->Done())
		return;
	std::shared_ptr<IHttpRequest> pPoll = m_pPoll;
	m_pPoll = nullptr;
	if(pPoll->State() != EHttpState::DONE)
		return;
	json_value *pJson = pPoll->ResultJson();
	const int64_t Now = time_get();
	if(pJson && pJson->type == json_object)
	{
		for(unsigned i = 0; i < pJson->u.object.length; i++)
		{
			const char *pKey = pJson->u.object.values[i].name;
			const json_value *pEntry = pJson->u.object.values[i].value;
			if(str_comp(pKey, m_aOwnKey) == 0 || !pEntry || pEntry->type != json_object)
				continue;
			const json_value *pId = json_object_get(pEntry, "i");
			const json_value *pX = json_object_get(pEntry, "x");
			const json_value *pY = json_object_get(pEntry, "y");
			const json_value *pKind = json_object_get(pEntry, "k");
			const json_value *pName = json_object_get(pEntry, "n");
			if(pId == &json_value_none || pId->type != json_integer || pX == &json_value_none || pX->type != json_integer || pY == &json_value_none || pY->type != json_integer)
				continue;
			const int Id = (int)pId->u.integer;
			if(m_Seen.count(Id))
				continue;
			m_Seen.insert(Id);
			// the first answer is what was already there when you came
			if(!m_Baseline)
				continue;
			SMarker Marker;
			Marker.m_Id = Id;
			Marker.m_Pos = vec2((float)std::clamp<int64_t>(pX->u.integer, -10000000, 10000000), (float)std::clamp<int64_t>(pY->u.integer, -10000000, 10000000));
			Marker.m_Kind = pKind != &json_value_none && pKind->type == json_integer ? (int)std::clamp<int64_t>(pKind->u.integer, 0, NUM_KINDS - 1) : KIND_HERE;
			if(pName != &json_value_none && pName->type == json_string)
				str_copy(Marker.m_aName, pName->u.string.ptr);
			Marker.m_Born = Now;
			// one marker for each player
			m_vMarkers.erase(std::remove_if(m_vMarkers.begin(), m_vMarkers.end(), [&](const SMarker &Old) { return !Old.m_Own && str_comp(Old.m_aName, Marker.m_aName) == 0; }), m_vMarkers.end());
			m_vMarkers.push_back(Marker);
			if(g_Config.m_CcMarkerSound)
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_HOOK_ATTACH_PLAYER, 0.7f);
		}
		m_Baseline = true;
	}
	else if(pJson)
	{
		// nothing there yet is an answer too (null)
		m_Baseline = true;
	}
	if(pJson)
		json_value_free(pJson);
	if(m_Seen.size() > 512)
		m_Seen.clear();
}

void CCMarkers::OnReset()
{
	m_vMarkers.clear();
	m_Seen.clear();
	m_Baseline = false;
	m_aScope[0] = '\0';
	m_aOwnPath[0] = '\0';
	m_OwnRemoteUntil = 0;
	m_HasCrystalTeammate = false;
	if(m_pPoll)
		m_pPoll->Abort();
	m_pPoll = nullptr;
}

void CCMarkers::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE && NewState != IClient::STATE_ONLINE)
	{
		RemoveOwnRemote();
		OnReset();
	}
}

void CCMarkers::OnShutdown()
{
	RemoveOwnRemote();
}

void CCMarkers::OnUpdate()
{
	const int64_t Now = time_get();
	// markers are for a few seconds
	m_vMarkers.erase(std::remove_if(m_vMarkers.begin(), m_vMarkers.end(), [&](const SMarker &Marker) { return Now - Marker.m_Born > time_freq() * TTL_SECONDS; }), m_vMarkers.end());
	if(m_OwnRemoteUntil != 0 && Now > m_OwnRemoteUntil)
		RemoveOwnRemote();

	char aScope[64];
	const bool HasScope = ScopePath(aScope, sizeof(aScope));
	if(str_comp(aScope, m_aScope) != 0)
	{
		// another team or server: what was there is not for you
		RemoveOwnRemote();
		m_vMarkers.clear();
		m_Seen.clear();
		m_Baseline = false;
		if(m_pPoll)
			m_pPoll->Abort();
		m_pPoll = nullptr;
		str_copy(m_aScope, aScope);
		m_NextPoll = Now;
	}
	if(!HasScope)
		return;

	if(Now > m_NextTeamCheck)
	{
		m_NextTeamCheck = Now + time_freq();
		const int LocalId = GameClient()->m_aLocalIds[g_Config.m_ClDummy];
		const int Team = GameClient()->m_Teams.Team(LocalId);
		m_HasCrystalTeammate = false;
		for(int i = 0; i < MAX_CLIENTS && !m_HasCrystalTeammate; i++)
			m_HasCrystalTeammate = i != LocalId && GameClient()->m_aClients[i].m_Active && GameClient()->m_Teams.Team(i) == Team && GameClient()->m_CPresence.IsCrystal(i);
	}

	ReadPoll();
	if(m_HasCrystalTeammate && !m_pPoll && Now > m_NextPoll)
	{
		m_NextPoll = Now + time_freq();
		Poll(aScope);
	}
}

void CCMarkers::OnRender()
{
	if(!g_Config.m_CcMarkers || m_vMarkers.empty() || Client()->State() != IClient::STATE_ONLINE)
		return;

	const CScreenRect Previous = Graphics()->GetScreen();
	const CScreenRect World = Graphics()->MapScreenToWorld(
		GameClient()->m_Camera.m_Center.x, GameClient()->m_Camera.m_Center.y,
		100.0f, 100.0f, 100.0f, 0, 0,
		Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom);
	Graphics()->MapScreen(World);
	const float WorldW = World.m_BottomRight.x - World.m_TopLeft.x;
	const float WorldH = World.m_BottomRight.y - World.m_TopLeft.y;
	const int64_t Now = time_get();

	struct SOffscreen
	{
		vec2 m_Pos;
		ColorRGBA m_Color;
		const SMarker *m_pMarker;
		float m_Alpha;
	};
	std::vector<SOffscreen> vOffscreen;

	for(const SMarker &Marker : m_vMarkers)
	{
		const float Age = (float)((double)(Now - Marker.m_Born) / (double)time_freq());
		const float Alpha = std::clamp(Age / 0.2f, 0.0f, 1.0f) * std::clamp(((float)TTL_SECONDS - Age) / 1.0f, 0.0f, 1.0f);
		if(Alpha <= 0.0f)
			continue;
		const ColorRGBA Color = KindColor(Marker.m_Kind);
		const vec2 P = Marker.m_Pos;

		const bool Inside = P.x >= World.m_TopLeft.x && P.x <= World.m_BottomRight.x && P.y >= World.m_TopLeft.y && P.y <= World.m_BottomRight.y;
		if(!Inside)
		{
			vOffscreen.push_back({P, Color, &Marker, Alpha});
			continue;
		}

		// the spot: a soft disc and a pulse that goes out
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		const float Pulse = std::fmod(Age, 1.2f) / 1.2f;
		Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha * 0.20f * (1.0f - Pulse));
		Graphics()->DrawCircle(P.x, P.y, 10.0f + 34.0f * Pulse, 32);
		Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha * 0.30f);
		Graphics()->DrawCircle(P.x, P.y, 14.0f, 24);
		Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha * 0.95f);
		Graphics()->DrawCircle(P.x, P.y, 4.5f, 16);
		// a beam up that fades out
		for(int i = 0; i < 8; i++)
		{
			Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha * 0.55f * (1.0f - i / 8.0f));
			const IGraphics::CQuadItem Beam(P.x - 2.0f, P.y - 10.0f - i * 9.0f - 9.0f, 4.0f, 9.0f);
			Graphics()->QuadsDrawTL(&Beam, 1);
		}
		// the diamond on the top of it
		Graphics()->QuadsSetRotation(pi / 4.0f);
		Graphics()->SetColor(Color.r, Color.g, Color.b, Alpha);
		const IGraphics::CQuadItem Diamond(P.x - 6.0f, P.y - 94.0f - 6.0f, 12.0f, 12.0f);
		Graphics()->QuadsDrawTL(&Diamond, 1);
		Graphics()->QuadsSetRotation(0.0f);
		Graphics()->QuadsEnd();

		// what it is, and who put it there
		const float Size = 12.0f;
		const char *pLabel = KindLabel(Marker.m_Kind);
		TextRender()->TextColor(Color.r, Color.g, Color.b, Alpha);
		TextRender()->Text(P.x - TextRender()->TextWidth(Size, pLabel) / 2.0f, P.y - 94.0f - 6.0f - Size - 14.0f, Size, pLabel);
		if(!Marker.m_Own && Marker.m_aName[0] != '\0')
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, Alpha * 0.85f);
			TextRender()->Text(P.x - TextRender()->TextWidth(9.0f, Marker.m_aName) / 2.0f, P.y - 94.0f - 6.0f - 11.0f, 9.0f, Marker.m_aName);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	// markers out of the screen: an arrow at the edge and how far it is
	if(!vOffscreen.empty())
	{
		const vec2 Center = (World.m_TopLeft + World.m_BottomRight) / 2.0f;
		const float Scale = std::max(WorldW, WorldH) / 300.0f; // the arrows keep their size when you zoom
		const float Inset = 26.0f * Scale;
		const float HalfW = WorldW / 2.0f - Inset;
		const float HalfH = WorldH / 2.0f - Inset;
		for(const SOffscreen &Off : vOffscreen)
		{
			vec2 Dir = Off.m_Pos - Center;
			const float Length = length(Dir);
			if(Length < 1.0f)
				continue;
			Dir /= Length;
			const float TX = std::abs(Dir.x) > 0.0001f ? HalfW / std::abs(Dir.x) : 1e9f;
			const float TY = std::abs(Dir.y) > 0.0001f ? HalfH / std::abs(Dir.y) : 1e9f;
			const vec2 Edge = Center + Dir * std::min(TX, TY);
			const vec2 Side(-Dir.y, Dir.x);

			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(Off.m_Color.r, Off.m_Color.g, Off.m_Color.b, Off.m_Alpha * 0.95f);
			const vec2 Tip = Edge + Dir * 9.0f * Scale;
			const vec2 BaseL = Edge - Dir * 6.0f * Scale + Side * 8.0f * Scale;
			const vec2 BaseR = Edge - Dir * 6.0f * Scale - Side * 8.0f * Scale;
			const IGraphics::CFreeformItem Arrow(Tip.x, Tip.y, Tip.x, Tip.y, BaseL.x, BaseL.y, BaseR.x, BaseR.y);
			Graphics()->QuadsDrawFreeform(&Arrow, 1);
			Graphics()->QuadsEnd();

			char aText[48];
			str_format(aText, sizeof(aText), "%s %dm", KindLabel(Off.m_pMarker->m_Kind), (int)(Length / 32.0f));
			const float Size = 9.0f * Scale;
			const vec2 TextPos = Edge - Dir * 20.0f * Scale;
			TextRender()->TextColor(Off.m_Color.r, Off.m_Color.g, Off.m_Color.b, Off.m_Alpha);
			TextRender()->Text(TextPos.x - TextRender()->TextWidth(Size, aText) / 2.0f, TextPos.y - Size / 2.0f, Size, aText);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
	}

	Graphics()->MapScreen(Previous);
}
