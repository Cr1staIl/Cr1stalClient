/* Cr1stal Client: counts how long you play every map, for the profile page. Based on DDNet, see licence.txt. */
#include "cclient_playtime.h"

#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/map.h>
#include <engine/serverbrowser.h>
#include <engine/shared/localization.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/race.h>
#include <game/localization.h>

#include <algorithm>
#include <cstdlib>

static const char *const s_pPlaytimeFile = "cclient_playtime.txt";

const char *CCPlaytime::CategoryName(int Category)
{
	switch(Category)
	{
	case CAT_EASY: return Localizable("Easy", "Playtime category");
	case CAT_MAIN: return Localizable("Main", "Playtime category");
	case CAT_HARD: return Localizable("Hard", "Playtime category");
	case CAT_INSANE: return Localizable("Insane", "Playtime category");
	case CAT_EXTREME: return Localizable("Extreme", "Playtime category");
	case CAT_MODS: return Localizable("Mods", "Playtime category");
	case CAT_JET: return Localizable("Jet", "Playtime category");
	case CAT_SOLO: return Localizable("Solo", "Playtime category");
	default: return Localizable("Other", "Playtime category");
	}
}

int CCPlaytime::CategoryFromServerName(const char *pServerName)
{
	// the more special words first, "Solo Insane" is Solo
	struct
	{
		const char *m_pWord;
		int m_Category;
	} s_aWords[] = {
		{"solo", CAT_SOLO},
		{"jet", CAT_JET},
		{"mod", CAT_MODS},
		{"extreme", CAT_EXTREME},
		{"insane", CAT_INSANE},
		{"hard", CAT_HARD},
		{"main", CAT_MAIN},
		{"easy", CAT_EASY},
	};
	for(const auto &Word : s_aWords)
		if(str_find_nocase(pServerName, Word.m_pWord))
			return Word.m_Category;
	return CAT_OTHER;
}

void CCPlaytime::FormatTime(int64_t Seconds, char *pBuf, int BufSize)
{
	const int64_t Days = Seconds / 86400;
	const int64_t Hours = Seconds / 3600 % 24;
	const int64_t Minutes = Seconds / 60 % 60;
	if(Days > 0)
		str_format(pBuf, BufSize, "%dd %dh", (int)Days, (int)Hours);
	else if(Hours > 0)
		str_format(pBuf, BufSize, "%dh %dm", (int)Hours, (int)Minutes);
	else if(Minutes > 0)
		str_format(pBuf, BufSize, "%dm", (int)Minutes);
	else
		str_format(pBuf, BufSize, "%ds", (int)Seconds);
}

int64_t CCPlaytime::Total() const
{
	int64_t Sum = 0;
	for(const SMap &Map : m_vMaps)
		Sum += Map.m_Seconds;
	return Sum;
}

int64_t CCPlaytime::CategorySeconds(int Category) const
{
	int64_t Sum = 0;
	for(const SMap &Map : m_vMaps)
		if(Map.m_Category == Category)
			Sum += Map.m_Seconds;
	return Sum;
}

std::vector<const CCPlaytime::SMap *> CCPlaytime::TopMaps(int Num) const
{
	std::vector<const SMap *> vpMaps;
	vpMaps.reserve(m_vMaps.size());
	for(const SMap &Map : m_vMaps)
		vpMaps.push_back(&Map);
	std::sort(vpMaps.begin(), vpMaps.end(), [](const SMap *pA, const SMap *pB) { return pA->m_Seconds > pB->m_Seconds; });
	if((int)vpMaps.size() > Num)
		vpMaps.resize(Num);
	return vpMaps;
}

CCPlaytime::SMap *CCPlaytime::FindOrAdd(const char *pName, int Category)
{
	for(SMap &Map : m_vMaps)
	{
		if(Map.m_Name == pName)
		{
			// a map played on a server with a known category keeps that category
			if(Map.m_Category == CAT_OTHER && Category != CAT_OTHER)
				Map.m_Category = Category;
			return &Map;
		}
	}
	SMap &Map = m_vMaps.emplace_back();
	Map.m_Name = pName;
	Map.m_Category = Category;
	return &Map;
}

void CCPlaytime::OnInit()
{
	Load();
	if(m_Since == 0)
	{
		m_Since = time_timestamp();
		m_Dirty = true;
	}
	m_LastTick = time_get();
	m_LastSave = time_get();
}

void CCPlaytime::OnUpdate()
{
	const int64_t Now = time_get();
	// a long pause (window dragged, breakpoint) counts as at most one second
	const double Delta = std::min((double)(Now - m_LastTick) / (double)time_freq(), 1.0);
	m_LastTick = Now;

	const bool Playing = Client()->State() == IClient::STATE_ONLINE &&
			     GameClient()->m_Snap.m_pLocalInfo &&
			     GameClient()->m_Snap.m_pLocalInfo->m_Team != TEAM_SPECTATORS &&
			     GameClient()->Map() && GameClient()->Map()->BaseName()[0] != '\0';
	if(!Playing)
	{
		m_Pending = 0.0;
	}
	else
	{
		m_Pending += Delta;
		if(m_Pending >= 1.0)
		{
			const int64_t Whole = (int64_t)m_Pending;
			m_Pending -= (double)Whole;
			SMap *pMap = FindOrAdd(GameClient()->Map()->BaseName(), CategoryFromServerName(Client()->ServerInfo().m_aName));
			pMap->m_Seconds += Whole;
			m_Dirty = true;
		}
	}

	if(m_Dirty && Now - m_LastSave > time_freq() * 30)
		Save();
}

void CCPlaytime::OnShutdown()
{
	if(m_Dirty)
		Save();
}

int CCPlaytime::CategoryPoints(int Category)
{
	switch(Category)
	{
	case CAT_EASY: return 5;
	case CAT_MAIN: return 10;
	case CAT_HARD: return 20;
	case CAT_INSANE: return 35;
	case CAT_EXTREME: return 50;
	case CAT_MODS:
	case CAT_JET:
	case CAT_SOLO: return 15;
	default: return 5;
	}
}

int CCPlaytime::Points() const
{
	int Points = 0;
	for(const SFinish &Finish : m_vFinishes)
		Points += CategoryPoints(Finish.m_Category);
	return Points;
}

void CCPlaytime::CategoryString(char *pBuf, int BufSize) const
{
	pBuf[0] = '\0';
	for(int i = 0; i < NUM_CATEGORIES; i++)
	{
		char aPart[24];
		str_format(aPart, sizeof(aPart), i == 0 ? "%lld" : ",%lld", (long long)CategorySeconds(i));
		str_append(pBuf, aPart, BufSize);
	}
}

void CCPlaytime::AddFinish(int TimeMs)
{
	if(Client()->State() != IClient::STATE_ONLINE || !GameClient()->Map())
		return;
	const char *pMap = GameClient()->Map()->BaseName();
	for(SFinish &Finish : m_vFinishes)
	{
		if(Finish.m_Map == pMap)
		{
			if(TimeMs > 0 && (Finish.m_BestMs <= 0 || TimeMs < Finish.m_BestMs))
				Finish.m_BestMs = TimeMs;
			m_Dirty = true;
			return;
		}
	}
	SFinish Finish;
	Finish.m_Map = pMap;
	Finish.m_Category = CategoryFromServerName(Client()->ServerInfo().m_aName);
	Finish.m_BestMs = std::max(0, TimeMs);
	Finish.m_When = time_timestamp();
	m_vFinishes.push_back(Finish);
	m_Dirty = true;
	Save();
}

void CCPlaytime::OnMessage(int MsgType, void *pRawMsg)
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0)
		return;
	if(MsgType == NETMSGTYPE_SV_RACEFINISH)
	{
		const CNetMsg_Sv_RaceFinish *pMsg = (CNetMsg_Sv_RaceFinish *)pRawMsg;
		if(pMsg->m_ClientId == LocalId)
			AddFinish(pMsg->m_Time);
	}
	else if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		// servers that only say it in the chat ("name finished in: 01:23.45")
		const CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
		if(pMsg->m_ClientId != -1)
			return;
		char aName[MAX_NAME_LENGTH];
		const int Time = CRaceHelper::TimeFromFinishMessage(pMsg->m_pMessage, aName, sizeof(aName));
		if(Time > 0 && str_comp(aName, GameClient()->m_aClients[LocalId].m_aName) == 0)
			AddFinish(Time);
	}
}

void CCPlaytime::Load()
{
	char *pFile = Storage()->ReadFileStr(s_pPlaytimeFile, IStorage::TYPE_SAVE);
	if(!pFile)
		return;

	// "since <unix time>" then one line per map: seconds <tab> category <tab> map name
	char *pLine = pFile;
	while(pLine && *pLine)
	{
		char *pEnd = (char *)str_find(pLine, "\n");
		if(pEnd)
			*pEnd = '\0';
		if(str_startswith(pLine, "since "))
		{
			m_Since = str_toint64_base(pLine + 6);
		}
		else if(str_startswith(pLine, "F\t"))
		{
			// F <tab> category <tab> best ms <tab> unix time <tab> map
			char *apParts[5] = {pLine};
			int Num = 1;
			for(char *p = pLine; *p && Num < 5; p++)
				if(*p == '\t')
				{
					*p = '\0';
					apParts[Num++] = p + 1;
				}
			if(Num == 5)
			{
				str_utf8_trim_right(apParts[4]);
				SFinish Finish;
				Finish.m_Category = std::clamp(str_toint(apParts[1]), 0, NUM_CATEGORIES - 1);
				Finish.m_BestMs = str_toint(apParts[2]);
				Finish.m_When = str_toint64_base(apParts[3]);
				Finish.m_Map = apParts[4];
				if(!Finish.m_Map.empty())
					m_vFinishes.push_back(Finish);
			}
		}
		else
		{
			char *pTab1 = (char *)str_find(pLine, "\t");
			char *pTab2 = pTab1 ? (char *)str_find(pTab1 + 1, "\t") : nullptr;
			if(pTab1 && pTab2)
			{
				*pTab1 = '\0';
				*pTab2 = '\0';
				str_utf8_trim_right(pTab2 + 1);
				const char *pName = pTab2 + 1;
				if(pName[0] != '\0')
				{
					SMap *pMap = FindOrAdd(pName, std::clamp(str_toint(pTab1 + 1), 0, NUM_CATEGORIES - 1));
					pMap->m_Seconds += std::max<int64_t>(0, str_toint64_base(pLine));
				}
			}
		}
		pLine = pEnd ? pEnd + 1 : nullptr;
	}
	free(pFile);
}

void CCPlaytime::Save()
{
	m_LastSave = time_get();
	IOHANDLE File = Storage()->OpenFile(s_pPlaytimeFile, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aLine[512];
	str_format(aLine, sizeof(aLine), "since %lld\n", (long long)m_Since);
	io_write(File, aLine, str_length(aLine));
	for(const SMap &Map : m_vMaps)
	{
		str_format(aLine, sizeof(aLine), "%lld\t%d\t%s\n", (long long)Map.m_Seconds, Map.m_Category, Map.m_Name.c_str());
		io_write(File, aLine, str_length(aLine));
	}
	for(const SFinish &Finish : m_vFinishes)
	{
		str_format(aLine, sizeof(aLine), "F\t%d\t%d\t%lld\t%s\n", Finish.m_Category, Finish.m_BestMs, (long long)Finish.m_When, Finish.m_Map.c_str());
		io_write(File, aLine, str_length(aLine));
	}
	io_close(File);
	m_Dirty = false;
}
