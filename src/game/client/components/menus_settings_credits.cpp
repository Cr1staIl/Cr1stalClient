#include "menus.h"

#include <engine/graphics.h>
#include <engine/textrender.h>

#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>

void CMenus::RenderSettingsCredits(CUIRect MainView)
{
	// C-Client: only the author of the client and the game it is built on
	static constexpr const char *const CREDITS =
		"\n"
		"Cr\xE2\x99\xB1stal Client is made by Cr\xE2\x99\xB1stal.\n"
		"\n";
	const float FontSize = 16.0f;

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 3.0f * FontSize;
	s_ScrollRegion.Begin(&MainView, &ScrollParams);

	const auto &&RenderCreditsLink = [&](const void *pLinkId, const char *pPrefix, const char *pLink, const char *pSuffix, const char *pUrl) {
		const float LineHeight = TextRender()->TextBoundingBox(FontSize, pLink).m_H;
		const float PrefixWidth = TextRender()->TextWidth(FontSize, pPrefix);
		const bool PrefixOwnLine = PrefixWidth + TextRender()->TextWidth(FontSize, pLink) + TextRender()->TextWidth(FontSize, pSuffix) > MainView.w;

		CUIRect Line, Prefix, LinkRect;
		MainView.HSplitTop(LineHeight, &Line, &MainView);
		s_ScrollRegion.AddRect(Line);
		if(PrefixOwnLine)
		{
			Ui()->DoLabel(&Line, pPrefix, FontSize, TEXTALIGN_TL);
			MainView.HSplitTop(LineHeight, &Line, &MainView);
			s_ScrollRegion.AddRect(Line);
		}
		else
		{
			Line.VSplitLeft(PrefixWidth, &Prefix, &Line);
			Ui()->DoLabel(&Prefix, pPrefix, FontSize, TEXTALIGN_TL);
		}

		Line.VSplitLeft(TextRender()->TextWidth(FontSize, pLink), &LinkRect, &Line);
		const ColorRGBA LinkColor = Ui()->HotItem() == pLinkId ? ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f) : ColorRGBA(0.4f, 0.7f, 1.0f, 1.0f);
		TextRender()->TextColor(LinkColor);
		Ui()->DoLabel(&LinkRect, pLink, FontSize, TEXTALIGN_TL);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		CUIRect Underline;
		LinkRect.HSplitBottom(1.0f, nullptr, &Underline);
		Underline.Draw(LinkColor, IGraphics::CORNER_NONE, 0.0f);

		if(Ui()->DoButtonLogic(pLinkId, 0, &LinkRect, BUTTONFLAG_LEFT))
		{
			Client()->ViewLink(pUrl);
		}

		Ui()->DoLabel(&Line, pSuffix, FontSize, TEXTALIGN_TL);
	};

	CTextCursor Cursor;
	Cursor.m_FontSize = FontSize;
	Cursor.m_LineWidth = MainView.w;

	const unsigned OldRenderFlags = TextRender()->GetRenderFlags();
	TextRender()->SetRenderFlags(OldRenderFlags | TEXT_RENDER_FLAG_ONE_TIME_USE);
	STextContainerIndex CreditsTextContainer;
	TextRender()->CreateTextContainer(CreditsTextContainer, &Cursor, CREDITS);
	TextRender()->SetRenderFlags(OldRenderFlags);
	if(CreditsTextContainer.Valid())
	{
		CUIRect CreditsLabel;
		MainView.HSplitTop(TextRender()->GetBoundingBoxTextContainer(CreditsTextContainer).m_H, &CreditsLabel, &MainView);
		s_ScrollRegion.AddRect(CreditsLabel);
		TextRender()->RenderTextContainer(CreditsTextContainer, TextRender()->DefaultTextColor(), TextRender()->DefaultTextOutlineColor(), CreditsLabel.x, CreditsLabel.y);
		TextRender()->DeleteTextContainer(CreditsTextContainer);
	}

	static char s_DDNetLinkId;
	RenderCreditsLink(&s_DDNetLinkId, "Built on ", "DDNet", ".", "https://ddnet.org/");

	s_ScrollRegion.End();
}
