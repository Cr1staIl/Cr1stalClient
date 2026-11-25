/* Cr1stal Client: what music is playing right now (Windows media controls). Based on DDNet, see licence.txt. */
#include "cclient_media.h"

#include <base/detect.h>

#include <algorithm>
#include <chrono>

#if defined(CONF_FAMILY_WINDOWS) && defined(_MSC_VER)
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>
#define CCLIENT_MEDIA_SUPPORTED 1
#endif

CMediaWatcher::~CMediaWatcher()
{
	Stop();
}

void CMediaWatcher::Start()
{
#if defined(CCLIENT_MEDIA_SUPPORTED)
	if(m_Started)
		return;
	m_Started = true;
	m_Stop = false;
	m_Thread = std::thread([this]() { Run(); });
#endif
}

void CMediaWatcher::Stop()
{
	if(!m_Started)
		return;
	m_Stop = true;
	if(m_Thread.joinable())
		m_Thread.join();
	m_Started = false;
}

CMediaWatcher::SInfo CMediaWatcher::Get()
{
	std::lock_guard<std::mutex> Lock(m_Mutex);
	return m_Info;
}

void CMediaWatcher::Run()
{
#if defined(CCLIENT_MEDIA_SUPPORTED)
	using namespace winrt;
	using namespace winrt::Windows::Media::Control;
	using namespace winrt::Windows::Graphics::Imaging;

	try
	{
		init_apartment(apartment_type::multi_threaded);
	}
	catch(...)
	{
	}

	GlobalSystemMediaTransportControlsSessionManager Manager{nullptr};
	std::string LastKey;
	int Version = 0;

	while(!m_Stop)
	{
		try
		{
			if(!Manager)
				Manager = GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

			SInfo Info;
			const auto Session = Manager.GetCurrentSession();
			if(Session)
			{
				const auto Properties = Session.TryGetMediaPropertiesAsync().get();
				Info.m_Title = to_string(Properties.Title());
				Info.m_Artist = to_string(Properties.Artist());
				Info.m_Playing = Session.GetPlaybackInfo().PlaybackStatus() == GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;
				Info.m_Valid = !Info.m_Title.empty();

				const std::string Key = Info.m_Title + "\n" + Info.m_Artist;
				if(Key != LastKey)
				{
					LastKey = Key;
					Version++;
					// cover picture, scaled down to 128x128
					const auto Thumbnail = Properties.Thumbnail();
					if(Thumbnail)
					{
						try
						{
							const auto Stream = Thumbnail.OpenReadAsync().get();
							const auto Decoder = BitmapDecoder::CreateAsync(Stream).get();
							// scale so the short side is 128 pixels and cut a square from the middle
							// (YouTube covers are 16:9, they would look squashed otherwise)
							const uint32_t SrcW = std::max<uint32_t>(1, Decoder.PixelWidth());
							const uint32_t SrcH = std::max<uint32_t>(1, Decoder.PixelHeight());
							const uint32_t Short = std::min(SrcW, SrcH);
							const uint32_t ScaledW = std::max<uint32_t>(128, SrcW * 128 / Short);
							const uint32_t ScaledH = std::max<uint32_t>(128, SrcH * 128 / Short);
							BitmapTransform Transform;
							Transform.ScaledWidth(ScaledW);
							Transform.ScaledHeight(ScaledH);
							Transform.Bounds(BitmapBounds{(ScaledW - 128) / 2, (ScaledH - 128) / 2, 128, 128});
							Transform.InterpolationMode(BitmapInterpolationMode::Fant);
							const auto Pixels = Decoder.GetPixelDataAsync(BitmapPixelFormat::Rgba8, BitmapAlphaMode::Straight, Transform,
											   ExifOrientationMode::RespectExifOrientation, ColorManagementMode::DoNotColorManage)
										    .get();
							const auto Data = Pixels.DetachPixelData();
							Info.m_vCover.assign(Data.begin(), Data.end());
							Info.m_CoverWidth = 128;
							Info.m_CoverHeight = 128;
						}
						catch(...)
						{
							Info.m_vCover.clear();
						}
					}
				}
				else
				{
					// same track: keep the cover we already have
					std::lock_guard<std::mutex> Lock(m_Mutex);
					Info.m_vCover = m_Info.m_vCover;
					Info.m_CoverWidth = m_Info.m_CoverWidth;
					Info.m_CoverHeight = m_Info.m_CoverHeight;
				}
			}
			else
			{
				LastKey.clear();
			}
			Info.m_Version = Version;
			{
				std::lock_guard<std::mutex> Lock(m_Mutex);
				m_Info = std::move(Info);
			}
		}
		catch(...)
		{
			Manager = nullptr; // try again later
		}

		for(int i = 0; i < 10 && !m_Stop; i++)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
#endif
}
