/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_START_H
#define GAME_CLIENT_COMPONENTS_MENUS_START_H

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CMenusStart : public CComponentInterfaces
{
public:
	void RenderStartMenu(CUIRect MainView);

private:
	bool CheckHotKey(int Key) const;
	void RenderNeonTitle(float CenterX, float Y);
	// macOS style dock: returns the index of the clicked item or -1
	int RenderDock(const CUIRect &MainView, const char *const *ppIcons, const char *const *ppLabels, const bool *pHighlighted, int Num);
};

#endif
