/* Cr1stal Client: your rank and points on KoG, for the profile page. Based on DDNet, see licence.txt. */
#include "cclient_kogstats.h"

#include <base/io.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/http.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cstdlib>
#include <string>

static const char *const s_pKogFile = "cclient_kog.txt";
// the page of a player on the KoG site: the new site first, then the main address
// (the old kog.tw has no such pages, the new site is moving there)
static const char *const s_apKogPlayerUrls[] = {
	"https://dev.kog.tw/players/%s",
	"https://kog.tw/players/%s",
};
static const int s_NumKogUrls = sizeof(s_apKogPlayerUrls) / sizeof(s_apKogPlayerUrls[0]);
static const int64_t s_StaleSeconds = 10 * 60;

namespace
{
// the whole number at the start of pText, "16,797" and "16797" both give 16797
bool ReadNumber(const char *pText, int *pOut)
{
	while(*pText == ' ' || *pText == '\t' || *pText == '"' || *pText == '#')
		pText++;
	if(*pText < '0' || *pText > '9')
		return false;
	long long Value = 0;
	for(; (*pText >= '0' && *pText <= '9') || *pText == ','; pText++)
	{
		if(*pText == ',')
			continue;
		Value = Value * 10 + (*pText - '0');
		if(Value > 2000000000LL)
			return false;
	}
	*pOut = (int)Value;
	return true;
}

// {"name":"Rank","value":168}: the value that belongs to the property called pKey
bool FindProperty(const char *pPage, const char *pKey, int *pOut)
{
	char aNeedle[64];
	str_format(aNeedle, sizeof(aNeedle), "\"name\":\"%s\"", pKey);
	const char *pFound = str_find(pPage, aNeedle);
	if(!pFound)
	{
		str_format(aNeedle, sizeof(aNeedle), "\"name\": \"%s\"", pKey);
		pFound = str_find(pPage, aNeedle);
	}
	if(!pFound)
		return false;
	const char *pValue = str_find(pFound, "\"value\"");
	// the value follows the name directly, a far one belongs to something else
	if(!pValue || pValue - pFound > 80)
		return false;
	pValue += 7;
	while(*pValue == ' ' || *pValue == ':')
		pValue++;
	return ReadNumber(pValue, pOut);
}
} // namespace

bool CCKogStats::ParsePage(const char *pPage, int *pRank, int *pPoints)
{
	int Rank = 0, Points = 0;
	// the structured data of the page first
	bool HasPoints = FindProperty(pPage, "Total points", &Points) || FindProperty(pPage, "Points", &Points);
	FindProperty(pPage, "Rank", &Rank);
	if(!HasPoints)
	{
		// the title: "name on King of Gores: ranked #168 with 16,797 points"
		const char *pRanked = str_find(pPage, "ranked #");
		if(pRanked && ReadNumber(pRanked + 8, &Rank))
		{
			const char *pWith = str_find(pRanked, " with ");
			HasPoints = pWith && ReadNumber(pWith + 6, &Points);
		}
	}
	if(!HasPoints)
		return false;
	*pRank = std::max(0, Rank);
	*pPoints = std::max(0, Points);
	return true;
}

bool CCKogStats::KnownForName(const char *pName) const
{
	return Known() && str_comp(m_aName, pName) == 0;
}

void CCKogStats::Load()
{
	char *pFile = Storage()->ReadFileStr(s_pKogFile, IStorage::TYPE_SAVE);
	if(!pFile)
		return;
	// one line: points <tab> rank <tab> unix time <tab> name
	char *apParts[4] = {pFile};
	int Num = 1;
	for(char *p = pFile; *p && *p != '\n' && Num < 4; p++)
		if(*p == '\t')
		{
			*p = '\0';
			apParts[Num++] = p + 1;
		}
	if(Num == 4)
	{
		char *pEnd = (char *)str_find(apParts[3], "\n");
		if(pEnd)
			*pEnd = '\0';
		m_Points = std::max(-1, str_toint(apParts[0]));
		m_Rank = std::max(0, str_toint(apParts[1]));
		m_FetchedAt = str_toint64_base(apParts[2]);
		str_copy(m_aName, apParts[3]);
	}
	free(pFile);
}

void CCKogStats::Save()
{
	m_Dirty = false;
	if(m_Points < 0)
		return;
	IOHANDLE File = Storage()->OpenFile(s_pKogFile, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;
	char aLine[256];
	str_format(aLine, sizeof(aLine), "%d\t%d\t%lld\t%s\n", m_Points, m_Rank, (long long)m_FetchedAt, m_aName);
	io_write(File, aLine, str_length(aLine));
	io_close(File);
}

void CCKogStats::OnInit()
{
	Load();
}

void CCKogStats::OnShutdown()
{
	if(m_Dirty)
		Save();
}

void CCKogStats::Request(bool Force)
{
	if(m_pRequest)
		return;
	const char *pName = g_Config.m_PlayerName;
	if(pName[0] == '\0')
		return;
	const bool SameName = str_comp(m_aName, pName) == 0;
	if(!Force && SameName && Known() && time_timestamp() - m_FetchedAt < s_StaleSeconds)
		return;
	// after a failed try, wait a minute (the page asks for this every frame)
	if(!Force && time_timestamp() - m_LastTryAt < 60)
		return;
	m_LastTryAt = time_timestamp();
	str_copy(m_aRequestedName, pName);
	m_Failed = false;
	m_NotFound = true;
	StartAttempt(0);
}

void CCKogStats::StartAttempt(int Attempt)
{
	m_Attempt = Attempt;
	char aEscaped[256];
	EscapeUrl(aEscaped, sizeof(aEscaped), m_aRequestedName);
	char aUrl[512];
	str_format(aUrl, sizeof(aUrl), s_apKogPlayerUrls[Attempt], aEscaped);
	std::shared_ptr<IHttpRequest> pRequest = HttpGet(aUrl);
	pRequest->Timeout(CTimeout{8000, 0, 500, 5});
	pRequest->LogProgress(HTTPLOG::FAILURE);
	pRequest->FailOnErrorStatus(false);
	Http()->Run(pRequest);
	m_pRequest = std::move(pRequest);
}

void CCKogStats::OnUpdate()
{
	if(m_pRequest && m_pRequest->Done())
	{
		bool Ok = false;
		if(m_pRequest->State() == EHttpState::DONE && m_pRequest->StatusCode() == 200)
		{
			unsigned char *pData = nullptr;
			size_t Size = 0;
			m_pRequest->Result(&pData, &Size);
			if(pData && Size > 0)
			{
				// a page is not null terminated
				const std::string Page((const char *)pData, std::min<size_t>(Size, 2 * 1024 * 1024));
				int Rank = 0, Points = 0;
				if(ParsePage(Page.c_str(), &Rank, &Points))
				{
					m_Rank = Rank;
					m_Points = Points;
					m_FetchedAt = time_timestamp();
					str_copy(m_aName, m_aRequestedName);
					Ok = true;
					Save();
				}
			}
		}
		// a page that does not know the name is no network problem
		if(!Ok && !(m_pRequest->State() == EHttpState::DONE && m_pRequest->StatusCode() == 404))
			m_NotFound = false;
		m_pRequest = nullptr;
		if(!Ok && m_Attempt + 1 < s_NumKogUrls)
			StartAttempt(m_Attempt + 1);
		else
		{
			m_Failed = !Ok;
			if(Ok)
				m_NotFound = false;
		}
	}
	// the reply to /points stays for a minute, then it is old news
	if(m_aReply[0] != '\0' && time_timestamp() - m_AskedAt > 60 && m_AskedAt != 0)
		m_aReply[0] = '\0';
}

bool CCKogStats::AskServer()
{
	if(Client()->State() != IClient::STATE_ONLINE || str_comp(Client()->ServerInfo().m_aCommunityId, "kog") != 0)
		return false;
	m_aReply[0] = '\0';
	m_AskedAt = time_timestamp();
	Console()->ExecuteLine("say /points", IConsole::CLIENT_ID_UNSPECIFIED);
	return true;
}

void CCKogStats::OnMessage(int MsgType, void *pRawMsg)
{
	// only the answer to our own question: a message of the server in the first seconds after it
	if(MsgType != NETMSGTYPE_SV_CHAT || m_AskedAt == 0 || time_timestamp() - m_AskedAt > 5 || m_aReply[0] != '\0')
		return;
	const CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;
	if(pMsg->m_ClientId != -1 || !str_find_nocase(pMsg->m_pMessage, "point"))
		return;
	str_copy(m_aReply, pMsg->m_pMessage);
}
