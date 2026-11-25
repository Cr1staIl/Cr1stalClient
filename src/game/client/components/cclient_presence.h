/* Cr1stal Client: who on the server also uses Cr1stal Client, and their game packs. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_PRESENCE_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_PRESENCE_H

#include <base/types.h>
#include <base/vmath.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

class IHttpRequest;

// Every Cr1stal Client player writes a small note to our database while being
// on a server: a hash of the server, a hash and the text of the name, and the
// hash of their game pack. Then everyone reads the notes of the server they are on.
// The packs are uploaded once and are found by their sha256, so the picture you
// download is exactly the one the other player uses.
class CCPresence : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnShutdown() override;

	// this player uses Cr1stal Client (you always do)
	bool IsCrystal(int ClientId) const;
	// hash of the other player's game pack when it is downloaded, else nullptr
	const char *PackOf(int ClientId) const;

	// the client icon as a small avatar, the badge of Cr1stal Client players
	void RenderBadge(vec2 Center, float Size, float Alpha) const;

private:
	struct SEntry
	{
		std::string m_Name;
		std::string m_Key;
		char m_aPack[17] = "";
		int64_t m_Time = 0;
	};

	char m_aServer[17] = "";
	char m_aaPublished[2][17] = {"", ""};
	int64_t m_NextPublish = 0;
	int64_t m_NextPoll = 0;
	int64_t m_NextMatch = 0;

	std::vector<SEntry> m_vEntries;
	bool m_aCrystal[MAX_CLIENTS] = {};
	char m_aaPack[MAX_CLIENTS][17] = {};

	// your own pack
	char m_aOwnFile[IO_MAX_PATH_LENGTH] = "";
	char m_aOwnPack[17] = "";
	std::string m_OwnPackData; // base64, only until it is uploaded
	bool m_OwnPackChecked = false;

	// packs of others
	std::set<std::string> m_ReadyPacks;
	std::set<std::string> m_FailedPacks;
	std::string m_DownloadingPack;

	std::shared_ptr<IHttpRequest> m_pPoll;
	std::shared_ptr<IHttpRequest> m_pPackCheck;
	std::shared_ptr<IHttpRequest> m_pPackDownload;
	std::vector<std::shared_ptr<IHttpRequest>> m_vpSent; // fire and forget

	void Send(const char *pPath, const char *pMethod, const char *pBody);
	void Publish();
	void Unpublish();
	void Poll();
	void ReadPoll();
	void Match();
	void UpdateOwnPack();
	void UpdatePackCheck();
	void UpdateDownload();
};

#endif
