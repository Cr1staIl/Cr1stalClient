/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_MENUS_START_H
#define GAME_CLIENT_COMPONENTS_MENUS_START_H

#include <engine/textrender.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

class CMenusStart : public CComponentInterfaces
{
public:
	void RenderStartMenu(CUIRect MainView);
	// forget the cached title letters (window size or font changed)
	void ResetTitle();

private:
	bool CheckHotKey(int Key) const;
	void RenderNeonTitle(float CenterX, float Y);

	static constexpr int MAX_TITLE_CHARS = 32;
	STextContainerIndex m_aTitleContainers[MAX_TITLE_CHARS];
	float m_aTitleWidths[MAX_TITLE_CHARS] = {};
	float m_aTitleSizes[MAX_TITLE_CHARS] = {};
	bool m_aTitleSpace[MAX_TITLE_CHARS] = {};
	int m_NumTitleChars = 0;
	// macOS style dock: returns the index of the clicked item or -1
	int RenderDock(const CUIRect &MainView, const char *const *ppIcons, const char *const *ppLabels, const bool *pHighlighted, int Num);
};

#endif
