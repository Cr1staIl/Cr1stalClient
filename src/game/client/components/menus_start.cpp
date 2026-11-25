/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "menus_start.h"

#include <engine/client/updater.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/serverbrowser.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/client_data.h>

#include <game/client/components/cclient.h>
#include <game/client/components/cclient_style.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>
#include <game/version.h>

#include <algorithm>
#include <cmath>
#include <string>

#if defined(CONF_PLATFORM_ANDROID)
#include <android/android_main.h>
#endif

void CMenusStart::RenderNeonTitle(float CenterX, float Y)
{
	auto LerpColor = [](const ColorRGBA &A, const ColorRGBA &B, float T) {
		return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
	};
	static const char *const s_pTitle = "Cr\xe2\x99\xb1stal Client";
	constexpr int CrossCodepoint = 0x2671;
	constexpr float FontSize = 58.0f;
	constexpr float CrossSize = FontSize * 1.45f; // the cross is drawn bigger than the letters
	constexpr float LetterSpacing = 1.0f;
	const float Time = Client()->GlobalTime();

	const ColorRGBA aPalette[] = {
		ColorRGBA(1.0f, 0.93f, 0.95f, 1.0f), // almost white
		ColorRGBA(1.0f, 0.35f, 0.45f, 1.0f), // light red
		ColorRGBA(0.88f, 0.07f, 0.24f, 1.0f), // crimson
	};

	const ColorRGBA OldColor = TextRender()->GetTextColor();
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	TextRender()->SetFontPreset(EFontPreset::TITLE_FONT);

	struct SChar
	{
		char m_aText[8];
		float m_Size;
		float m_Width;
	};
	SChar aChars[32];
	int NumChars = 0;
	float TotalWidth = 0.0f;
	const char *pCur = s_pTitle;
	while(*pCur && NumChars < (int)std::size(aChars))
	{
		const char *pStart = pCur;
		const int Code = str_utf8_decode(&pCur);
		if(Code <= 0)
			break;
		SChar &Char = aChars[NumChars++];
		str_copy(Char.m_aText, pStart, std::min<int>(sizeof(Char.m_aText), pCur - pStart + 1));
		Char.m_Size = Code == CrossCodepoint ? CrossSize : FontSize;
		Char.m_Width = TextRender()->TextWidth(Char.m_Size, Char.m_aText) + LetterSpacing;
		TotalWidth += Char.m_Width;
	}

	float X = CenterX - TotalWidth / 2.0f;
	for(int Index = 0; Index < NumChars; Index++)
	{
		const SChar &Char = aChars[Index];

		// fixed colour gradient through the title: cyan -> violet -> magenta
		const float T = NumChars > 1 ? Index / (float)(NumChars - 1) : 0.0f;
		const ColorRGBA Color = T < 0.5f ? LerpColor(aPalette[0], aPalette[1], T * 2.0f) : LerpColor(aPalette[1], aPalette[2], (T - 0.5f) * 2.0f);

		// every letter floats gently up/down and sways a little, each with its own rhythm
		const float BobY = std::sin(Time * 1.3f + Index * 0.55f) * 3.0f;
		const float SwayX = std::sin(Time * 0.7f + Index * 1.1f) * 1.2f;
		const float CharX = X + SwayX;
		const float CharY = Y - (Char.m_Size - FontSize) * 0.55f + BobY;

		// soft halo behind the letter: many faint circles make a smooth falloff
		if(Char.m_aText[0] != ' ')
		{
			const vec2 HaloCenter(CharX + Char.m_Width / 2.0f, CharY + Char.m_Size * 0.55f);
			Graphics()->TextureClear();
			Graphics()->BlendAdditive();
			Graphics()->QuadsBegin();
			for(int Ring = 20; Ring >= 1; Ring--)
			{
				Graphics()->SetColor(aPalette[2].WithAlpha(0.005f));
				Graphics()->DrawCircle(HaloCenter.x, HaloCenter.y, Char.m_Size * 0.55f * Ring / 20.0f, 48);
			}
			Graphics()->QuadsEnd();
			Graphics()->BlendNormal();
		}

		// crisp letter with a thin neon outline (no shifted copies, so no jagged steps)
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(CharX, CharY));
		Cursor.m_FontSize = Char.m_Size;
		STextContainerIndex Container;
		TextRender()->CreateTextContainer(Container, &Cursor, Char.m_aText);
		if(Container.Valid())
		{
			TextRender()->RenderTextContainer(Container, LerpColor(Color, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), 0.45f), Color.WithAlpha(0.55f), 0.0f, 0.0f);
			TextRender()->DeleteTextContainer(Container);
		}

		X += Char.m_Width;
	}

	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	TextRender()->TextColor(OldColor);
}

void CMenusStart::RenderStartMenu(CUIRect MainView)
{
	GameClient()->m_MenuBackground.ChangePosition(CMenuBackground::POS_START);

	// render the neon "Cr♱stal Client" title
	RenderNeonTitle(MainView.w / 2, 70.0f);

	// Cr1stal Client in-game update button
	{
		CCClient &CClient = GameClient()->m_CClient;
		const CCClient::EUpdateState State = CClient.UpdateState();
		if(State == CCClient::EUpdateState::AVAILABLE || State == CCClient::EUpdateState::FAILED ||
			State == CCClient::EUpdateState::DOWNLOADING || State == CCClient::EUpdateState::INSTALLING)
		{
			CUIRect UpdateButton;
			UpdateButton.x = MainView.w / 2.0f - 120.0f;
			UpdateButton.y = 150.0f;
			UpdateButton.w = 240.0f;
			UpdateButton.h = 26.0f;
			char aLabel[128];
			if(State == CCClient::EUpdateState::DOWNLOADING || State == CCClient::EUpdateState::INSTALLING)
			{
				Ui()->RenderProgressBar(UpdateButton, CClient.UpdateProgress());
				str_format(aLabel, sizeof(aLabel), "%s %d%%", Localize("Downloading update"), round_to_int(CClient.UpdateProgress() * 100.0f));
				Ui()->DoLabel(&UpdateButton, aLabel, 12.0f, TEXTALIGN_MC);
			}
			else
			{
				if(State == CCClient::EUpdateState::FAILED)
					str_copy(aLabel, Localize("Update failed, try again"));
				else
					str_format(aLabel, sizeof(aLabel), "%s (#%d)", Localize("Update Cr1stal Client"), CClient.LatestBuild());
				static CButtonContainer s_UpdateButton;
				const float Pulse = 0.5f + 0.5f * std::sin(Client()->GlobalTime() * 3.0f);
				if(GameClient()->m_Menus.DoButton_Menu(&s_UpdateButton, aLabel, 0, &UpdateButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 13.0f, 0.0f, ColorRGBA(0.204f, 0.78f, 0.349f, 0.3f + 0.2f * Pulse)))
					CClient.StartUpdate();
			}
		}
	}

	const float VMargin = MainView.w / 2 - 190.0f;

	CUIRect Button;
	int NewPage = -1;

	// small links in the bottom left corner
	{
		CUIRect Links;
		MainView.VSplitLeft(30.0f, nullptr, &Links);
		Links.VSplitLeft(110.0f, &Links, nullptr);
		Links.HSplitBottom(25.0f, &Links, nullptr);
		static CButtonContainer s_DiscordButton, s_LearnButton, s_TutorialButton, s_WebsiteButton;
		struct
		{
			CButtonContainer *m_pButton;
			const char *m_pText;
		} aLinks[] = {
			{&s_DiscordButton, Localize("Discord")},
			{&s_LearnButton, Localize("Learn")},
			{&s_TutorialButton, Localize("Tutorial")},
			{&s_WebsiteButton, Localize("Website")},
		};
		for(int i = 0; i < (int)std::size(aLinks); i++)
		{
			Links.HSplitBottom(18.0f, &Links, &Button);
			Links.HSplitBottom(3.0f, &Links, nullptr);
			const float HoverA = Ui()->HoverAnim(aLinks[i].m_pButton);
			Button.VSplitLeft(4.0f * HoverA, nullptr, &Button); // slides a little to the right
			SLabelProperties Props;
			Props.SetColor(ColorRGBA(0.62f + 0.38f * HoverA, 0.6f - 0.4f * HoverA, 0.65f - 0.35f * HoverA, 0.75f + 0.25f * HoverA));
			Ui()->DoLabel(&Button, aLinks[i].m_pText, 11.0f, TEXTALIGN_ML, Props);
			if(Ui()->DoButtonLogic(aLinks[i].m_pButton, 0, &Button, BUTTONFLAG_LEFT))
			{
				if(i == 0)
					Client()->ViewLink(Localize("https://ddnet.org/discord"));
				else if(i == 1)
					Client()->ViewLink(Localize("https://wiki.ddnet.org/"));
				else if(i == 2)
					GameClient()->m_Menus.JoinTutorial();
				else
					Client()->ViewLink("https://ddnet.org/");
			}
		}
	}

	// big play button under the title
	{
		static CButtonContainer s_PlayButton;
		const float HoverA = Ui()->HoverAnim(&s_PlayButton);
		const float Grow = 4.0f * HoverA;
		CUIRect Play;
		Play.w = 220.0f + Grow * 2.0f;
		Play.h = 42.0f + Grow;
		Play.x = MainView.w / 2.0f - Play.w / 2.0f;
		Play.y = 190.0f - Grow / 2.0f;
		// soft red light around it, stronger on hover
		for(int i = 4; i >= 1; i--)
		{
			CUIRect Halo;
			Play.Margin(-2.0f * i, &Halo);
			Halo.Draw(ColorRGBA(0.86f, 0.07f, 0.23f, (0.02f + 0.035f * HoverA)), IGraphics::CORNER_ALL, Halo.h / 2.0f);
		}
		Play.Draw4(ColorRGBA(0.95f, 0.16f, 0.32f, 1.0f), ColorRGBA(0.72f, 0.04f, 0.17f, 1.0f), ColorRGBA(0.8f, 0.07f, 0.22f, 1.0f), ColorRGBA(0.55f, 0.02f, 0.12f, 1.0f), IGraphics::CORNER_ALL, Play.h / 2.0f);
		if(HoverA > 0.01f)
			Play.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.08f * HoverA), IGraphics::CORNER_ALL, Play.h / 2.0f);
		SLabelProperties Props;
		Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		Ui()->DoLabel(&Play, Localize("Play", "Start menu"), 17.0f + HoverA, TEXTALIGN_MC, Props);
		if(Ui()->DoButtonLogic(&s_PlayButton, 0, &Play, BUTTONFLAG_LEFT) || Ui()->ConsumeHotkey(CUi::HOTKEY_ENTER) || CheckHotKey(KEY_P))
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
	}

	// dock at the bottom
	{
		const bool LocalServerRunning = GameClient()->m_LocalServer.IsServerRunning();
		enum
		{
			DOCK_PLAY = 0,
			DOCK_DEMOS,
			DOCK_EDITOR,
			DOCK_SERVER,
			DOCK_PROFILE,
			DOCK_NEWS,
			DOCK_SETTINGS,
			DOCK_QUIT,
			NUM_DOCK,
		};
		const char *apIcons[NUM_DOCK] = {FontIcon::PLAY, FontIcon::FILM, FontIcon::PEN_TO_SQUARE, LocalServerRunning ? FontIcon::STOP : FontIcon::NETWORK_WIRED, FontIcon::USER, FontIcon::NEWSPAPER, FontIcon::GEAR, FontIcon::POWER_OFF};
		const char *apLabels[NUM_DOCK] = {Localize("Play", "Start menu"), Localize("Demos"), Localize("Editor"), LocalServerRunning ? Localize("Stop server") : Localize("Run server"), Localize("Profile"), Localize("News"), Localize("Settings"), Localize("Quit")};
		const bool aHighlighted[NUM_DOCK] = {false, false, GameClient()->Editor()->HasUnsavedData(), LocalServerRunning, false, g_Config.m_UiUnreadNews != 0, false, false};
		const int Clicked = RenderDock(MainView, apIcons, apLabels, aHighlighted, NUM_DOCK);

		bool UsedEscape = false;
		if(Clicked == DOCK_DEMOS || CheckHotKey(KEY_D))
			NewPage = CMenus::PAGE_DEMOS;
		if(Clicked == DOCK_PLAY)
			NewPage = g_Config.m_UiPage >= CMenus::PAGE_INTERNET && g_Config.m_UiPage <= CMenus::PAGE_FAVORITE_COMMUNITY_5 ? g_Config.m_UiPage : CMenus::PAGE_INTERNET;
		if(Clicked == DOCK_EDITOR || CheckHotKey(KEY_E))
		{
			g_Config.m_ClEditor = 1;
			Input()->MouseModeRelative();
		}
		if(Clicked == DOCK_SERVER || CheckHotKey(KEY_R))
		{
			if(LocalServerRunning)
				GameClient()->m_LocalServer.KillServer();
			else
				GameClient()->m_LocalServer.RunServer({});
		}
		if(LocalServerRunning && CheckHotKey(KEY_J))
			GameClient()->m_LocalServer.Connect();
		if(Clicked == DOCK_PROFILE)
		{
			g_Config.m_UiSettingsPage = CMenus::SETTINGS_PLAYER;
			NewPage = CMenus::PAGE_SETTINGS;
		}
		if(Clicked == DOCK_NEWS || CheckHotKey(KEY_N))
			NewPage = CMenus::PAGE_NEWS;
		if(Clicked == DOCK_SETTINGS || CheckHotKey(KEY_S))
			NewPage = CMenus::PAGE_SETTINGS;
		if(Clicked == DOCK_QUIT || (UsedEscape = Ui()->ConsumeHotkey(CUi::HOTKEY_ESCAPE)) || CheckHotKey(KEY_Q))
		{
			if(UsedEscape || GameClient()->Editor()->HasUnsavedData() || (GameClient()->CurrentRaceTime() / 60 >= g_Config.m_ClConfirmQuitTime && g_Config.m_ClConfirmQuitTime >= 0))
				GameClient()->m_Menus.ShowQuitPopup();
			else
				Client()->Quit();
		}
	}

	// render version
	CUIRect CurVersion, ConsoleButton;
	MainView.HSplitBottom(45.0f, nullptr, &CurVersion);
	CurVersion.VSplitRight(40.0f, &CurVersion, nullptr);
	CurVersion.HSplitTop(20.0f, &ConsoleButton, &CurVersion);
	CurVersion.HSplitTop(5.0f, nullptr, &CurVersion);
	ConsoleButton.VSplitRight(40.0f, nullptr, &ConsoleButton);
	// version in the corner, with a quiet credit to DDNet
	SLabelProperties VersionLabelProps;
	VersionLabelProps.SetColor(ColorRGBA(0.8f, 0.8f, 1.0f, 0.35f));
	Ui()->DoLabel(&CurVersion, "based on DDNet " GAME_RELEASE_VERSION, 10.0f, TEXTALIGN_MR, VersionLabelProps);

	static CButtonContainer s_ConsoleButton;
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	if(GameClient()->m_Menus.DoButton_Menu(&s_ConsoleButton, FontIcon::TERMINAL, 0, &ConsoleButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.1f)))
	{
		GameClient()->m_GameConsole.Toggle(CGameConsole::CONSOLETYPE_LOCAL);
	}
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	CUIRect VersionUpdate;
	MainView.HSplitBottom(20.0f, nullptr, &VersionUpdate);
	VersionUpdate.VMargin(VMargin, &VersionUpdate);
#if defined(CONF_AUTOUPDATE)
	CUIRect UpdateButton;
	VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &UpdateButton);
	VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

	char aBuf[128];
	const IUpdater::EUpdaterState State = Updater()->GetCurrentState();
	const bool NeedUpdate = str_comp(Client()->LatestVersion(), "0");

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Update now"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)))
		{
			Updater()->InitiateUpdate();
		}
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		static CButtonContainer s_VersionUpdate;
		if(GameClient()->m_Menus.DoButton_Menu(&s_VersionUpdate, Localize("Restart"), 0, &UpdateButton, BUTTONFLAG_LEFT, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)))
		{
			Client()->Restart();
		}
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		Ui()->RenderProgressBar(UpdateButton, Updater()->GetCurrentPercent() / 100.0f);
	}

	if(State == IUpdater::CLEAN && NeedUpdate)
	{
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::CLEAN)
	{
		aBuf[0] = '\0';
	}
	else if(State >= IUpdater::GETTING_MANIFEST && State < IUpdater::NEED_RESTART)
	{
		char aCurrentFile[64];
		Updater()->GetCurrentFile(aCurrentFile, sizeof(aCurrentFile));
		str_format(aBuf, sizeof(aBuf), Localize("Downloading %s:"), aCurrentFile);
	}
	else if(State == IUpdater::FAIL)
	{
		str_copy(aBuf, Localize("Update failed! Check log…"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	else if(State == IUpdater::NEED_RESTART)
	{
		str_copy(aBuf, Localize("DDNet Client updated!"));
		TextRender()->TextColor(1.0f, 0.4f, 0.4f, 1.0f);
	}
	Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
#elif defined(CONF_INFORM_UPDATE)
	if(str_comp(Client()->LatestVersion(), "0") != 0)
	{
		CUIRect DownloadButton;
		VersionUpdate.VSplitRight(100.0f, &VersionUpdate, &DownloadButton);
		VersionUpdate.VSplitRight(10.0f, &VersionUpdate, nullptr);

		static CButtonContainer s_DownloadButton;
		if(GameClient()->m_Menus.DoButton_Menu(&s_DownloadButton, Localize("Download"), 0, &DownloadButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.07f)))
		{
			Client()->ViewLink("https://ddnet.org/downloads/");
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("DDNet %s is out!"), Client()->LatestVersion());
		SLabelProperties UpdateLabelProps;
		UpdateLabelProps.SetColor(ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoLabel(&VersionUpdate, aBuf, 14.0f, TEXTALIGN_ML, UpdateLabelProps);
	}
#endif

	if(NewPage != -1)
	{
		GameClient()->m_Menus.SetShowStart(false);
		GameClient()->m_Menus.SetMenuPage(NewPage);
	}
}

int CMenusStart::RenderDock(const CUIRect &MainView, const char *const *ppIcons, const char *const *ppLabels, const bool *pHighlighted, int Num)
{
	static CButtonContainer s_aButtons[16];
	static char s_aLabelKeys[16]; // own addresses for the label fade
	static char s_DockKey;
	Num = std::min(Num, (int)std::size(s_aButtons));

	const float BaseSize = 30.0f;
	const float Gap = 7.0f;
	const float Pad = 7.0f;
	const float MaxGrow = 0.5f; // the icon under the mouse becomes 50% bigger
	const float Reach = 52.0f; // how far the growing spreads to the neighbours
	const float BottomY = MainView.y + MainView.h - 22.0f;

	// the dock without growing, to know where the mouse is relative to the icons
	const float BaseWidth = Num * BaseSize + (Num - 1) * Gap;
	const float BaseX = MainView.x + MainView.w / 2.0f - BaseWidth / 2.0f;
	CUIRect Panel;
	Panel.x = BaseX - Pad;
	Panel.y = BottomY - BaseSize - Pad;
	Panel.w = BaseWidth + Pad * 2.0f;
	Panel.h = BaseSize + Pad * 2.0f;
	// the grown icons stick out above the panel, they still count as the dock
	CUIRect HoverZone = Panel;
	HoverZone.y -= BaseSize * MaxGrow + 6.0f;
	HoverZone.h += BaseSize * MaxGrow + 6.0f;
	HoverZone.x -= BaseSize * MaxGrow;
	HoverZone.w += BaseSize * MaxGrow * 2.0f;
	const float DockA = Ui()->StateAnim(&s_DockKey, Ui()->MouseInside(&HoverZone) && !Ui()->IsPopupOpen(), 10.0f);
	const float MouseX = Ui()->MouseX();

	float aScale[16];
	float Total = 0.0f;
	for(int i = 0; i < Num; i++)
	{
		const float Center = BaseX + i * (BaseSize + Gap) + BaseSize / 2.0f;
		const float Dist = (MouseX - Center) / Reach;
		aScale[i] = 1.0f + MaxGrow * DockA * std::exp(-Dist * Dist);
		Total += BaseSize * aScale[i];
	}
	Total += (Num - 1) * Gap;

	// panel grows with the icons, like on a Mac
	Panel.w = Total + Pad * 2.0f;
	Panel.x = MainView.x + MainView.w / 2.0f - Panel.w / 2.0f;
	CCStyle::DrawCard(Panel, 12.0f, ColorRGBA(0.07f, 0.065f, 0.075f, 0.92f));

	int Clicked = -1;
	float X = Panel.x + Pad;
	for(int i = 0; i < Num; i++)
	{
		const float Size = BaseSize * aScale[i];
		CUIRect Icon;
		Icon.x = X;
		Icon.y = BottomY - Size;
		Icon.w = Size;
		Icon.h = Size;
		X += Size + Gap;

		const float HoverA = Ui()->HoverAnim(&s_aButtons[i]);
		CCStyle::DrawCard(Icon, 7.0f + 2.0f * (aScale[i] - 1.0f), ColorRGBA(0.1f, 0.095f, 0.105f, 1.0f), HoverA);
		if(pHighlighted[i])
		{
			// small dot under an icon that wants attention
			CUIRect Dot;
			Dot.w = Dot.h = 3.0f;
			Dot.x = Icon.x + Icon.w / 2.0f - 1.5f;
			Dot.y = Icon.y + Icon.h + 2.5f;
			Dot.Draw(CCStyle::ACCENT, IGraphics::CORNER_ALL, 1.5f);
		}

		TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
		TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
		SLabelProperties IconProps;
		IconProps.SetColor(CCStyle::Mix(ColorRGBA(0.85f, 0.12f, 0.3f, 0.85f), ColorRGBA(1.0f, 0.38f, 0.5f, 1.0f), HoverA));
		Ui()->DoLabel(&Icon, ppIcons[i], Size * 0.42f, TEXTALIGN_MC, IconProps);
		TextRender()->SetRenderFlags(0);
		TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

		// label above the icon, fades in and rises a little
		const float LabelA = Ui()->StateAnim(&s_aLabelKeys[i], Ui()->HotItem() == &s_aButtons[i], 14.0f);
		if(LabelA > 0.01f)
		{
			const float FontSize = 8.5f;
			const std::string Text = CCStyle::Upper(ppLabels[i]);
			CUIRect Label;
			Label.w = TextRender()->TextWidth(FontSize, Text.c_str()) + 12.0f;
			Label.h = 14.0f;
			Label.x = Icon.x + Icon.w / 2.0f - Label.w / 2.0f;
			Label.y = Icon.y - Label.h - 5.0f - 4.0f * LabelA;
			Label.Draw(ColorRGBA(0.12f, 0.03f, 0.06f, 0.95f * LabelA), IGraphics::CORNER_ALL, 4.0f);
			SLabelProperties Props;
			Props.SetColor(CCStyle::ACCENT_TEXT.WithAlpha(LabelA));
			Ui()->DoLabel(&Label, Text.c_str(), FontSize, TEXTALIGN_MC, Props);
		}

		if(Ui()->DoButtonLogic(&s_aButtons[i], 0, &Icon, BUTTONFLAG_LEFT))
			Clicked = i;
	}
	return Clicked;
}

bool CMenusStart::CheckHotKey(int Key) const
{
	return !Input()->ShiftIsPressed() && !Input()->ModifierIsPressed() && !Input()->AltIsPressed() && // no modifier
	       Input()->KeyPress(Key) &&
	       !GameClient()->m_GameConsole.IsActive();
}
