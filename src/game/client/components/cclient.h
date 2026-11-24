/* Cr1stal Client (C-Client): save wheel and map markers. Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_H

#include <base/vmath.h>

#include <engine/console.h>
#include <engine/graphics.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

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
	static const char *SaveName(int Save);

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

	bool m_WheelActive = false;
	vec2 m_SelectorMouse = vec2(0.0f, 0.0f);
	int m_SelectedSave = -1;

	SMarker m_aMarkers[MAX_MARKERS];
	int m_NextMarker = 0;

	int m_aSaveRequest[MAX_CLIENTS];
	float m_aSaveRequestTime[MAX_CLIENTS];

	float m_LastSendTime = -100.0f;

	static void ConPing(IConsole::IResult *pResult, void *pUserData);

	int LocalTeam() const;
	bool CanSend();
	void SendPing();
	void SendSave(int Save);
	void AddMarker(vec2 Pos, int ClientId);
	void ShowSave(int ClientId, int Save);

	void RenderWorld();
	void RenderWheel();
};

#endif
