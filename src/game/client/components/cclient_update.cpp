/* Cr1stal Client: own updater. Based on DDNet, see licence.txt. */
#include "cclient_update.h"

#include <base/io.h>
#include <base/log.h>
#include <base/process.h>
#include <base/str.h>

#include <engine/client.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/cclient_version.h>
#include <game/client/gameclient.h>

#include <iterator>

static const char *const UPDATE_BASE_URL = "https://github.com/Cr1staIl/Cr1stalClient/releases/download/test-latest/";
static const char *const UPDATE_FILE = "cclient_update/Cr1stalClient-Setup.exe";

int CCUpdate::CurrentBuild()
{
	return CCLIENT_BUILD;
}

bool CCUpdate::CanSelfUpdate() const
{
#if defined(CONF_FAMILY_WINDOWS)
	return CurrentBuild() > 0;
#else
	return false;
#endif
}

void CCUpdate::OnInit()
{
	if(g_Config.m_CcUpdateCheck)
		Check();
}

void CCUpdate::Check()
{
	if(!CanSelfUpdate() || m_State == EState::CHECKING || m_State == EState::DOWNLOADING || m_State == EState::INSTALLING)
		return;
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%sversion.json", UPDATE_BASE_URL);
	m_pVersionRequest = HttpGet(aUrl);
	m_pVersionRequest->Timeout(CTimeout{10000, 0, 500, 10});
	m_pVersionRequest->MaxResponseSize(16 * 1024);
	m_pVersionRequest->LogProgress(HTTPLOG::FAILURE);
	Http()->Run(m_pVersionRequest);
	m_State = EState::CHECKING;
}

void CCUpdate::Start()
{
	if(m_State != EState::AVAILABLE && m_State != EState::FAILED)
		return;
	Storage()->CreateFolder("cclient_update", IStorage::TYPE_SAVE);
	char aUrl[256];
	str_format(aUrl, sizeof(aUrl), "%sCr1stalClient-Setup.exe", UPDATE_BASE_URL);
	m_pSetupRequest = HttpGetFile(aUrl, Storage(), UPDATE_FILE, IStorage::TYPE_SAVE);
	m_pSetupRequest->Timeout(CTimeout{10000, 0, 500, 30});
	m_pSetupRequest->MaxResponseSize(300 * 1024 * 1024);
	Http()->Run(m_pSetupRequest);
	m_State = EState::DOWNLOADING;
}

float CCUpdate::Progress() const
{
	if(m_State == EState::DOWNLOADING && m_pSetupRequest)
		return m_pSetupRequest->Progress() / 100.0f;
	return m_State == EState::INSTALLING ? 1.0f : 0.0f;
}

void CCUpdate::OnUpdate()
{
	if(m_State == EState::CHECKING && m_pVersionRequest && m_pVersionRequest->Done())
	{
		m_State = EState::FAILED;
		if(m_pVersionRequest->State() == EHttpState::DONE)
		{
			json_value *pJson = m_pVersionRequest->ResultJson();
			if(pJson)
			{
				const json_value *pBuild = json_object_get(pJson, "build");
				if(pBuild && pBuild->type == json_integer)
				{
					m_LatestBuild = json_int_get(pBuild);
					m_State = m_LatestBuild > CurrentBuild() ? EState::AVAILABLE : EState::UP_TO_DATE;
				}
				json_value_free(pJson);
			}
		}
		if(m_State == EState::FAILED)
			m_State = EState::NONE; // no button when the check simply did not work
		m_pVersionRequest = nullptr;
	}

	if(m_State == EState::DOWNLOADING && m_pSetupRequest && m_pSetupRequest->Done())
	{
		if(m_pSetupRequest->State() == EHttpState::DONE)
			Finish();
		else
			m_State = EState::FAILED;
		m_pSetupRequest = nullptr;
	}
}

void CCUpdate::Finish()
{
	char aPath[IO_MAX_PATH_LENGTH];
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, UPDATE_FILE, aPath, sizeof(aPath));
	// the installer closes the game, installs over it and starts it again
	const char *apArgs[] = {"/SILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS"};
	const PROCESS Process = process_execute(aPath, EShellExecuteWindowState::FOREGROUND, apArgs, std::size(apArgs));
	if(Process == INVALID_PROCESS)
	{
		log_error("cclient", "could not start the installer '%s'", aPath);
		m_State = EState::FAILED;
		return;
	}
	m_State = EState::INSTALLING;
	Client()->Quit();
}

void CCUpdate::OnShutdown()
{
	if(m_pVersionRequest)
		m_pVersionRequest->Abort();
	if(m_pSetupRequest && m_State == EState::DOWNLOADING)
		m_pSetupRequest->Abort();
}
