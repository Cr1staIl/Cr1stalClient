/* Cr1stal Client: your rank and points on KoG, for the profile page. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_KOGSTATS_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_KOGSTATS_H

#include <game/client/component.h>

#include <cstdint>
#include <memory>
#include <string>

class IHttpRequest;

// Asks the KoG site for the rank and points of your nickname and keeps the last answer in a file, so the
// profile has something to show offline. The answer of the server to /points is kept as it is.
class CCKogStats : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;
	void OnShutdown() override;
	void OnMessage(int MsgType, void *pRawMsg) override;

	// the numbers of the last answer, for the name the answer was for
	bool Known() const { return m_Points >= 0; }
	bool KnownForName(const char *pName) const;
	int Points() const { return m_Points; }
	// 0 when the site did not tell
	int Rank() const { return m_Rank; }
	bool Loading() const { return m_pRequest != nullptr; }
	bool Failed() const { return m_Failed; }
	// the site answered but has no player with this name
	bool NotFound() const { return m_Failed && m_NotFound; }
	// unix time of the last answer, 0 if there is none
	int64_t FetchedAt() const { return m_FetchedAt; }
	const char *Name() const { return m_aName; }

	// asks for the name you play with now. Without Force only when the last answer is old or for another name.
	void Request(bool Force);
	// says /points on a KoG server, the answer shows up in LastReply()
	bool AskServer();
	const char *LastReply() const { return m_aReply; }

	// the numbers of a page of the site (used by the request, public to be easy to test)
	static bool ParsePage(const char *pPage, int *pRank, int *pPoints);

private:
	int m_Points = -1;
	int m_Rank = 0;
	bool m_Failed = false;
	bool m_NotFound = false;
	int m_Attempt = 0;
	int64_t m_FetchedAt = 0;
	char m_aName[16 * 4] = "";
	std::shared_ptr<IHttpRequest> m_pRequest;
	char m_aRequestedName[16 * 4] = "";
	char m_aReply[256] = "";
	int64_t m_AskedAt = 0;
	int64_t m_LastTryAt = 0;
	bool m_Dirty = false;

	void StartAttempt(int Attempt);
	void Load();
	void Save();
};

#endif
