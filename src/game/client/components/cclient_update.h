/* Cr1stal Client: own updater. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_UPDATE_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_UPDATE_H

#include <game/client/component.h>

#include <memory>

class IHttpRequest;

// Windows: looks at the test-latest release of the repository, and when it has a newer build
// than the running one, downloads the installer and runs it silently. The installer closes
// the game, installs over it and starts it again.
class CCUpdate : public CComponent
{
public:
	enum class EState
	{
		NONE,
		CHECKING,
		UP_TO_DATE,
		AVAILABLE,
		DOWNLOADING,
		INSTALLING,
		FAILED,
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnUpdate() override;
	void OnShutdown() override;

	EState State() const { return m_State; }
	int LatestBuild() const { return m_LatestBuild; }
	static int CurrentBuild();
	float Progress() const;
	// only Windows builds made by GitHub know their number
	bool CanSelfUpdate() const;
	void Check();
	void Start();

private:
	EState m_State = EState::NONE;
	int m_LatestBuild = 0;
	std::shared_ptr<IHttpRequest> m_pVersionRequest;
	std::shared_ptr<IHttpRequest> m_pSetupRequest;
	void Finish();
};

#endif
