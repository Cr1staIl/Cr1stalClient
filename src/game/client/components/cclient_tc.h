/* Cr1stal Client: small features ported from TClient (https://github.com/sjrc6/TaterClient-ddnet). Based on DDNet, see licence.txt. */
#ifndef GAME_CLIENT_COMPONENTS_CCLIENT_TC_H
#define GAME_CLIENT_COMPONENTS_CCLIENT_TC_H

#include <base/color.h>
#include <base/vmath.h>

#include <engine/client/enums.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

// rainbow color for the tee, the hook and the weapon (cc_rainbow_*)
class CCRainbow : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;

	ColorRGBA m_RainbowColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

private:
	float m_Time = 0.0f;
	ColorHSLA m_RandomFrom = ColorHSLA(0.0f, 0.0f, 0.0f, 0.0f);
	ColorHSLA m_RandomTo = ColorHSLA(0.0f, 0.0f, 0.0f, 0.0f);
	float m_LastSwap = -1000.0f;
};

// auto reply, auto vote "no" when you are far, commands on connect/join, name near finish
class CCAutomation : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnStateChange(int NewState, int OldState) override;
	void OnNewSnapshot() override;
	void OnRender() override;
	void OnInit() override;

private:
	class IEngineGraphics *m_pEngineGraphics = nullptr;
	char m_aPreviousOwnMessage[256] = "";
	int m_aSnapshotsSinceJoin[NUM_DUMMIES] = {};
	bool m_aJoinDone[NUM_DUMMIES] = {};
	float m_FinishCheckTimeout = 0.0f;

	bool SendNonDuplicateMessage(int Team, const char *pLine);
	void DoFinishCheck();
};

// a little tee flying next to yours (cc_pet_*)
class CCPet : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnMapLoad() override;

private:
	vec2 m_Target = vec2(0.0f, 0.0f);
	vec2 m_Position = vec2(0.0f, 0.0f);
	vec2 m_Velocity = vec2(0.0f, 0.0f);
	vec2 m_Dir = vec2(1.0f, 0.0f);
	float m_Alpha = 0.0f;
};

#endif
