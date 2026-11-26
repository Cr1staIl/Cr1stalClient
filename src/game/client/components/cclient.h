/* Cr1stal Client (C-Client): save wheel and map markers. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/image.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/client/components/cclient_media.h>

#include <memory>
#include <string>
#include <vector>

class IHttpRequest;

// Save wheel: hold Shift (emote wheel) and press Q or E, then pick how your
// teammates should save you. Map markers: press the marker key to point at a
// spot for your team. Both are sent as team chat messages with a short tag,
// so other Cr1stal Client players see a picture or a marker, everyone else
// still reads a normal chat message.
class CCClient : public CComponent
{
public:
	enum
	{
		SAVE_HOOK_UP = 0,
		SAVE_HOOK_RIGHT,
		SAVE_HAMMER,
		SAVE_STAND_UNDER,
		SAVE_HOOK_DOWN,
		SAVE_HOOK_LEFT,
		SAVE_WAIT,
		SAVE_THANKS,
		NUM_SAVES,
	};

	CCClient();
	int Sizeof() const override { return sizeof(*this); }

	void OnConsoleInit() override;
	void OnInit() override;
	void OnReset() override;
	void OnRelease() override;
	void OnRender() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	bool IsWheelActive() const { return m_WheelActive; }

	// in-game updater (Windows): checks the test-latest release and installs it with one click
	enum class EUpdateState
	{
		NONE,
		CHECKING,
		UP_TO_DATE,
		AVAILABLE,
		DOWNLOADING,
		INSTALLING,
		FAILED,
	};
	EUpdateState UpdateState() const { return m_UpdateState; }
	int LatestBuild() const { return m_LatestBuild; }
	static int CurrentBuild();
	float UpdateProgress() const;
	bool CanSelfUpdate() const;
	void CheckForUpdate();
	void StartUpdate();
	void OnUpdate() override;
	void OnShutdown() override;
	static const char *SaveName(int Save);

	// glowing picture behind your tee (cc_aura 1..NUM_AURAS)
	static constexpr int NUM_AURAS = 4;
	void RenderAura(vec2 Pos);
	IGraphics::CTextureHandle AuraTexture(int Index) const { return m_aAuraTextures[Index]; }
	IGraphics::CTextureHandle IconTexture() const { return m_IconTexture; }
	IGraphics::CTextureHandle BadgeTexture() const { return m_BadgeTexture; }

	// music island at the top of the screen (called by the HUD, HUD coordinates)
	void RenderMusicIsland(float ScreenWidth);

	// frames per second, measured over the last second
	int Fps() const { return m_Fps; }
	// cc_optimize: true while the "weaker PC" settings are used
	bool OptimizeStrong() const { return m_OptimizeStrong; }

	// fonts for all text
	static constexpr int NUM_FONTS = 11;
	static const char *FontFamily(int Font);
	static const char *FontDisplayName(int Font);

private:
	struct SMarker
	{
		vec2 m_Pos;
		int m_ClientId;
		float m_Time;
	};
	static constexpr int MAX_MARKERS = 16;
	static constexpr float MARKER_LIFETIME = 6.0f;
	static constexpr float SAVE_LIFETIME = 5.0f;

	IGraphics::CTextureHandle m_aSaveTextures[NUM_SAVES];
	IGraphics::CTextureHandle m_aAuraTextures[NUM_AURAS];
	int m_AppliedFont = -1;

	CMediaWatcher m_Media;
	CMediaWatcher::SInfo m_MediaInfo;
	IGraphics::CTextureHandle m_CoverTexture;
	IGraphics::CTextureHandle m_IconTexture;
	IGraphics::CTextureHandle m_BadgeTexture; // the client sign without its plate, next to names
	int m_CoverVersion = -1;
	float m_NextMediaPoll = 0.0f;
	float m_IslandWidth = 0.0f; // animated
	float m_IslandVisible = 0.0f; // animated 0..1
	std::string m_IslandTitle; // title the scrolling started for
	float m_IslandTitleStart = 0.0f;
	void UpdateMedia();

	void ApplyFont();

	int m_Fps = 0;
	int m_FpsFrames = 0;
	float m_FpsTime = 0.0f;
	int m_LastOptimize = -1;
	bool m_OptimizeStrong = false;
	void UpdateOptimize();

	bool m_WheelActive = false;
	bool m_WheelFromBind = false; // opened with +cc_save_wheel, closes when that key is released
	vec2 m_SelectorMouse = vec2(0.0f, 0.0f);
	int m_SelectedSave = -1;
	float m_aWheelAnim[NUM_SAVES] = {};

	SMarker m_aMarkers[MAX_MARKERS];
	int m_NextMarker = 0;

	int m_aSaveRequest[MAX_CLIENTS];
	float m_aSaveRequestTime[MAX_CLIENTS];

	float m_LastSendTime = -100.0f;

	EUpdateState m_UpdateState = EUpdateState::NONE;
	int m_LatestBuild = 0;
	std::shared_ptr<IHttpRequest> m_pVersionRequest;
	std::shared_ptr<IHttpRequest> m_pSetupRequest;
	void FinishUpdate();

	static void ConPing(IConsole::IResult *pResult, void *pUserData);

	int LocalTeam() const;
	bool CanSend();
	void SendPing();
	void SendSave(int Save);
	void AddMarker(vec2 Pos, int ClientId);
	void ShowSave(int ClientId, int Save);

	void RenderWorld();
	void RenderWheel();
	static void ConSaveWheel(IConsole::IResult *pResult, void *pUserData);
	static void ConSave(IConsole::IResult *pResult, void *pUserData);
};

// Outlines around freeze, wall, unfreeze and death tiles (drawn above the map, below the tees)
class CCOutlines : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
