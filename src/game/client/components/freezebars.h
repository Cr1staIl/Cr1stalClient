#ifndef GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#define GAME_CLIENT_COMPONENTS_FREEZEBARS_H
#include <base/vmath.h>

#include <game/client/component.h>

class CFreezeBars : public CComponent
{
	void RenderFreezeBar(int ClientId);
	void RenderFreezeBarPos(float x, float y, float Width, float Height, float Progress, float Alpha = 1.0f);
	bool IsPlayerInfoAvailable(int ClientId) const;
	void RenderFreezeLine(vec2 TeePos, float Progress, float Seconds, float Alpha);

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
};

#endif
