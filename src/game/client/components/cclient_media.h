/* Cr1stal Client: what music is playing right now (Windows media controls). Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_MEDIA_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_MEDIA_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Reads the "now playing" info that Windows shows in its media overlay
// (YouTube in the browser, Spotify, ...). Runs on its own thread, the game
// only copies the latest result. On other systems it never finds anything.
class CMediaWatcher
{
public:
	struct SInfo
	{
		std::string m_Title;
		std::string m_Artist;
		bool m_Playing = false;
		bool m_Valid = false;
		// cover picture, RGBA
		std::vector<uint8_t> m_vCover;
		int m_CoverWidth = 0;
		int m_CoverHeight = 0;
		int m_Version = 0; // changes when the track changes
	};

	~CMediaWatcher();
	void Start();
	void Stop();
	SInfo Get();

private:
	void Run();

	std::mutex m_Mutex;
	SInfo m_Info;
	std::thread m_Thread;
	std::atomic<bool> m_Stop = false;
	bool m_Started = false;
};

#endif
