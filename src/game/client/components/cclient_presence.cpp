/* Cr1stal Client: who on the server also uses Cr1stal Client, and their game packs. Based on DDNet, see licence.txt. */
#include "cclient_presence.h"

#include "cclient_style.h"

#include <base/hash.h>
#include <base/io.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/net.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <cmath>

static const char *const DATABASE_URL = "https://cr1stal-client-default-rtdb.europe-west1.firebasedatabase.app";
static const char *const PACK_FOLDER = "cclient_packs";
static constexpr int MAX_PACK_BYTES = 2900 * 1024; // the database takes 4 MB of base64
static constexpr int64_t FRESH_MS = 100 * 1000; // notes older than this belong to players that left

// first 16 hex chars of the sha256, with a prefix so the hashes are only ours
static void ShortHash(char (&aOut)[17], const char *pText)
{
	char aSalted[256];
	str_format(aSalted, sizeof(aSalted), "cr1stal:%s", pText);
	const SHA256_DIGEST Digest = sha256(aSalted, str_length(aSalted));
	char aStr[SHA256_MAXSTRSIZE];
	sha256_str(Digest, aStr, sizeof(aStr));
	str_copy(aOut, aStr);
}

static bool IsHash(const char *pText)
{
	if(str_length(pText) != 16)
		return false;
	for(const char *p = pText; *p; p++)
		if(!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')))
			return false;
	return true;
}

void CCPresence::OnInit()
{
	Storage()->CreateFolder(PACK_FOLDER, IStorage::TYPE_SAVE);
}

void CCPresence::Send(const char *pPath, const char *pMethod, const char *pBody)
{
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%s/%s.json", DATABASE_URL, pPath);
	std::shared_ptr<IHttpRequest> pRequest = HttpPostJson(aUrl, pBody);
	pRequest->HeaderString("X-HTTP-Method-Override", pMethod);
	pRequest->Timeout(CTimeout{5000, 60000, 500, 10});
	pRequest->MaxResponseSize(4096);
	pRequest->LogProgress(HTTPLOG::NONE);
	Http()->Run(pRequest);
	m_vpSent.push_back(pRequest);
}

void CCPresence::OnStateChange(int NewState, int OldState)
{
	if(OldState == IClient::STATE_ONLINE && NewState != IClient::STATE_ONLINE)
	{
		Unpublish();
		m_aServer[0] = '\0';
		m_vEntries.clear();
		if(m_pPoll)
			m_pPoll->Abort();
		m_pPoll = nullptr;
		Match();
	}
}

void CCPresence::OnShutdown()
{
	Unpublish();
	for(auto &[Hash, Skin] : GameClient()->m_CcPackSkins)
		if(Skin.m_Loaded)
			GameClient()->UnloadGameSkin(Skin);
	GameClient()->m_CcPackSkins.clear();
}

void CCPresence::Unpublish()
{
	if(m_aServer[0] == '\0')
		return;
	for(auto &aPublished : m_aaPublished)
	{
		if(aPublished[0] == '\0')
			continue;
		char aPath[64];
		str_format(aPath, sizeof(aPath), "p/%s/%s", m_aServer, aPublished);
		Send(aPath, "DELETE", "null");
		aPublished[0] = '\0';
	}
}

void CCPresence::Publish()
{
	for(int Dummy = 0; Dummy < NUM_DUMMIES; Dummy++)
	{
		char aKey[17] = "";
		const int LocalId = GameClient()->m_aLocalIds[Dummy];
		const bool Connected = Dummy == 0 || Client()->DummyConnected();
		if(g_Config.m_CcSharePresence && Connected && LocalId >= 0 && LocalId < MAX_CLIENTS && GameClient()->m_aClients[LocalId].m_aName[0])
		{
			const char *pName = GameClient()->m_aClients[LocalId].m_aName;
			ShortHash(aKey, pName);
			char aPath[64];
			str_format(aPath, sizeof(aPath), "p/%s/%s", m_aServer, aKey);
			char aEscaped[128];
			char aBody[256];
			if(m_aOwnPack[0])
				str_format(aBody, sizeof(aBody), "{\"n\":\"%s\",\"h\":\"%s\",\"t\":{\".sv\":\"timestamp\"}}", EscapeJson(aEscaped, sizeof(aEscaped), pName), m_aOwnPack);
			else
				str_format(aBody, sizeof(aBody), "{\"n\":\"%s\",\"t\":{\".sv\":\"timestamp\"}}", EscapeJson(aEscaped, sizeof(aEscaped), pName));
			Send(aPath, "PUT", aBody);
		}
		// renamed, left with the dummy or stopped sharing: remove the old note
		if(m_aaPublished[Dummy][0] && str_comp(m_aaPublished[Dummy], aKey) != 0)
		{
			char aPath[64];
			str_format(aPath, sizeof(aPath), "p/%s/%s", m_aServer, m_aaPublished[Dummy]);
			Send(aPath, "DELETE", "null");
		}
		str_copy(m_aaPublished[Dummy], aKey);
	}
}

void CCPresence::Poll()
{
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%s/p/%s.json", DATABASE_URL, m_aServer);
	m_pPoll = HttpGet(aUrl);
	m_pPoll->Timeout(CTimeout{5000, 15000, 500, 10});
	m_pPoll->MaxResponseSize(256 * 1024);
	m_pPoll->LogProgress(HTTPLOG::NONE);
	Http()->Run(m_pPoll);
}

void CCPresence::ReadPoll()
{
	if(!m_pPoll || !m_pPoll->Done())
		return;
	std::shared_ptr<IHttpRequest> pPoll = m_pPoll;
	m_pPoll = nullptr;
	if(pPoll->State() != EHttpState::DONE)
		return;
	json_value *pJson = pPoll->ResultJson();
	m_vEntries.clear();
	if(pJson && pJson->type == json_object)
	{
		int64_t Newest = 0;
		for(unsigned i = 0; i < pJson->u.object.length; i++)
		{
			const json_value *pEntry = pJson->u.object.values[i].value;
			const json_value *pName = json_object_get(pEntry, "n");
			const json_value *pPack = json_object_get(pEntry, "h");
			const json_value *pTime = json_object_get(pEntry, "t");
			if(pName == &json_value_none || pName->type != json_string || pTime == &json_value_none || pTime->type != json_integer)
				continue;
			SEntry Entry;
			Entry.m_Name = pName->u.string.ptr;
			if(IsHash(pJson->u.object.values[i].name))
				Entry.m_Key = pJson->u.object.values[i].name;
			if(pPack != &json_value_none && pPack->type == json_string && IsHash(pPack->u.string.ptr))
				str_copy(Entry.m_aPack, pPack->u.string.ptr);
			Entry.m_Time = pTime->u.integer;
			Newest = std::max(Newest, Entry.m_Time);
			m_vEntries.push_back(Entry);
		}
		// players whose game crashed never removed their note, keep the list small
		int Removed = 0;
		for(const SEntry &Entry : m_vEntries)
		{
			if(!Entry.m_Key.empty() && Entry.m_Time < Newest - 10 * 60 * 1000 && Removed < 4)
			{
				char aPath[64];
				str_format(aPath, sizeof(aPath), "p/%s/%s", m_aServer, Entry.m_Key.c_str());
				Send(aPath, "DELETE", "null");
				Removed++;
			}
		}
		m_vEntries.erase(std::remove_if(m_vEntries.begin(), m_vEntries.end(), [Newest](const SEntry &Entry) { return Entry.m_Time < Newest - FRESH_MS; }), m_vEntries.end());
	}
	if(pJson)
		json_value_free(pJson);
	Match();
}

void CCPresence::Match()
{
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		m_aCrystal[i] = false;
		m_aaPack[i][0] = '\0';
		const auto &ClientData = GameClient()->m_aClients[i];
		if(!ClientData.m_Active)
			continue;
		if(i == GameClient()->m_aLocalIds[0] || (Client()->DummyConnected() && i == GameClient()->m_aLocalIds[1]))
		{
			m_aCrystal[i] = true;
			continue;
		}
		for(const SEntry &Entry : m_vEntries)
		{
			if(str_comp(Entry.m_Name.c_str(), ClientData.m_aName) != 0)
				continue;
			m_aCrystal[i] = true;
			if(Entry.m_aPack[0] && str_comp(Entry.m_aPack, m_aOwnPack) != 0)
				str_copy(m_aaPack[i], Entry.m_aPack);
			break;
		}
	}
}

bool CCPresence::IsCrystal(int ClientId) const
{
	return ClientId >= 0 && ClientId < MAX_CLIENTS && m_aCrystal[ClientId];
}

const char *CCPresence::PackOf(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !m_aaPack[ClientId][0])
		return nullptr;
	if(m_ReadyPacks.find(m_aaPack[ClientId]) == m_ReadyPacks.end())
		return nullptr;
	return m_aaPack[ClientId];
}

void CCPresence::UpdateOwnPack()
{
	if(str_comp(m_aOwnFile, GameClient()->m_aCcGameSkinFile) == 0)
		return;
	str_copy(m_aOwnFile, GameClient()->m_aCcGameSkinFile);
	m_aOwnPack[0] = '\0';
	m_OwnPackData.clear();
	m_OwnPackChecked = false;
	if(m_aOwnFile[0] == '\0')
		return;
	void *pData = nullptr;
	unsigned Size = 0;
	if(!Storage()->ReadFile(m_aOwnFile, IStorage::TYPE_ALL, &pData, &Size))
		return;
	if(Size > 8 && Size <= (unsigned)MAX_PACK_BYTES && mem_comp(pData, "\x89PNG\r\n\x1a\n", 8) == 0)
	{
		const SHA256_DIGEST Digest = sha256(pData, Size);
		char aStr[SHA256_MAXSTRSIZE];
		sha256_str(Digest, aStr, sizeof(aStr));
		str_copy(m_aOwnPack, aStr);
		std::vector<char> vBase64(Size * 4 / 3 + 8);
		str_base64(vBase64.data(), (int)vBase64.size(), pData, (int)Size);
		m_OwnPackData = vBase64.data();
	}
	free(pData);
	m_NextPublish = 0; // tell the others about the new pack soon
}

void CCPresence::UpdatePackCheck()
{
	// is your pack already in the database? upload it once if not
	if(m_aOwnPack[0] == '\0' || m_OwnPackChecked || !g_Config.m_CcSharePresence || m_aServer[0] == '\0')
		return;
	if(!m_pPackCheck)
	{
		char aUrl[256];
		str_format(aUrl, sizeof(aUrl), "%s/packs/%s.json?shallow=true", DATABASE_URL, m_aOwnPack);
		m_pPackCheck = HttpGet(aUrl);
		m_pPackCheck->Timeout(CTimeout{5000, 15000, 500, 10});
		m_pPackCheck->MaxResponseSize(1024);
		m_pPackCheck->LogProgress(HTTPLOG::NONE);
		Http()->Run(m_pPackCheck);
		return;
	}
	if(!m_pPackCheck->Done())
		return;
	std::shared_ptr<IHttpRequest> pCheck = m_pPackCheck;
	m_pPackCheck = nullptr;
	if(pCheck->State() != EHttpState::DONE)
	{
		m_OwnPackChecked = true; // try again with the next pack or next start
		return;
	}
	unsigned char *pResult = nullptr;
	size_t ResultLength = 0;
	pCheck->Result(&pResult, &ResultLength);
	const bool Missing = ResultLength >= 4 && str_comp_num((const char *)pResult, "null", 4) == 0;
	if(Missing && !m_OwnPackData.empty())
	{
		char aPath[64];
		str_format(aPath, sizeof(aPath), "packs/%s", m_aOwnPack);
		std::string Body = "\"" + m_OwnPackData + "\"";
		Send(aPath, "PUT", Body.c_str());
	}
	m_OwnPackChecked = true;
	m_OwnPackData.clear();
	m_OwnPackData.shrink_to_fit();
}

void CCPresence::UpdateDownload()
{
	if(m_pPackDownload)
	{
		if(!m_pPackDownload->Done())
			return;
		std::shared_ptr<IHttpRequest> pDownload = m_pPackDownload;
		m_pPackDownload = nullptr;
		const std::string Pack = m_DownloadingPack;
		m_DownloadingPack.clear();
		bool Ok = false;
		json_value *pJson = pDownload->State() == EHttpState::DONE ? pDownload->ResultJson() : nullptr;
		if(pJson && pJson->type == json_string)
		{
			std::vector<unsigned char> vData(pJson->u.string.length * 3 / 4 + 8);
			const int Size = str_base64_decode(vData.data(), (int)vData.size(), pJson->u.string.ptr);
			if(Size > 8 && mem_comp(vData.data(), "\x89PNG\r\n\x1a\n", 8) == 0)
			{
				// the name of the pack is its hash: only take exactly that picture
				const SHA256_DIGEST Digest = sha256(vData.data(), Size);
				char aStr[SHA256_MAXSTRSIZE];
				sha256_str(Digest, aStr, sizeof(aStr));
				if(str_comp_num(aStr, Pack.c_str(), 16) == 0)
				{
					char aFile[IO_MAX_PATH_LENGTH];
					str_format(aFile, sizeof(aFile), "%s/%s.png", PACK_FOLDER, Pack.c_str());
					IOHANDLE File = Storage()->OpenFile(aFile, IOFLAG_WRITE, IStorage::TYPE_SAVE);
					if(File)
					{
						Ok = io_write(File, vData.data(), Size) == (unsigned)Size;
						io_close(File);
					}
				}
			}
		}
		if(pJson)
			json_value_free(pJson);
		if(Ok)
			m_ReadyPacks.insert(Pack);
		else
			m_FailedPacks.insert(Pack);
		return;
	}

	if(!g_Config.m_CcOthersPacks)
		return;
	for(const auto &aPack : m_aaPack)
	{
		if(!aPack[0] || m_ReadyPacks.count(aPack) || m_FailedPacks.count(aPack))
			continue;
		char aFile[IO_MAX_PATH_LENGTH];
		str_format(aFile, sizeof(aFile), "%s/%s.png", PACK_FOLDER, aPack);
		if(Storage()->FileExists(aFile, IStorage::TYPE_SAVE))
		{
			m_ReadyPacks.insert(aPack);
			continue;
		}
		char aUrl[256];
		str_format(aUrl, sizeof(aUrl), "%s/packs/%s.json", DATABASE_URL, aPack);
		m_pPackDownload = HttpGet(aUrl);
		m_pPackDownload->Timeout(CTimeout{5000, 60000, 500, 10});
		m_pPackDownload->MaxResponseSize(4200 * 1024);
		m_pPackDownload->LogProgress(HTTPLOG::FAILURE);
		Http()->Run(m_pPackDownload);
		m_DownloadingPack = aPack;
		return;
	}
}

void CCPresence::OnUpdate()
{
	m_vpSent.erase(std::remove_if(m_vpSent.begin(), m_vpSent.end(), [](const std::shared_ptr<IHttpRequest> &pRequest) { return pRequest->Done(); }), m_vpSent.end());

	UpdateOwnPack();
	ReadPoll();
	UpdateDownload();

	const int64_t Now = time_get();
	if(Client()->State() != IClient::STATE_ONLINE)
	{
		if(Now > m_NextMatch)
		{
			Match();
			m_NextMatch = Now + time_freq();
		}
		return;
	}

	char aAddr[NETADDR_MAXSTRSIZE];
	net_addr_str(&Client()->ServerAddress(), aAddr, sizeof(aAddr), true);
	char aServer[17];
	ShortHash(aServer, aAddr);
	if(str_comp(aServer, m_aServer) != 0)
	{
		Unpublish();
		str_copy(m_aServer, aServer);
		m_vEntries.clear();
		m_NextPublish = Now + time_freq() * 2; // after the names arrive
		m_NextPoll = Now + time_freq() * 3;
	}

	UpdatePackCheck();

	if(Now > m_NextPublish)
	{
		Publish();
		m_NextPublish = Now + time_freq() * 30;
	}
	if(Now > m_NextPoll && !m_pPoll)
	{
		Poll();
		m_NextPoll = Now + time_freq() * 10;
	}
	// names change and players join between the polls
	if(Now > m_NextMatch)
	{
		Match();
		m_NextMatch = Now + time_freq();
	}
}

void CCPresence::RenderBadge(vec2 Center, float Size, float Alpha) const
{
	// the client icon, like a small avatar of Cr1stal Client
	const IGraphics::CTextureHandle Icon = GameClient()->m_CClient.IconTexture();
	if(!Icon.IsValid() || Icon.IsNullTexture())
		return;
	Graphics()->TextureSet(Icon);
	CCStyle::TexturedRoundRect(Graphics(), Center.x - Size / 2.0f, Center.y - Size / 2.0f, Size, Size, Size * 0.22f, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
}
