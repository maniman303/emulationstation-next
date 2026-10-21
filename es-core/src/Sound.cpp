#include "Sound.h"

#include "AudioManager.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"
#include "resources/ResourceManager.h"
#include <algorithm>

std::map< std::string, std::shared_ptr<Sound> > Sound::sMap;

std::shared_ptr<Sound> Sound::get(const std::string& path, float playbackRate)
{
	std::string file = ResourceManager::getInstance()->getResourcePath(path);
	playbackRate = std::max(0.5f, std::min(2.0f, playbackRate));
	const std::string key = file + "@" + std::to_string(playbackRate);

	auto it = sMap.find(key);
	if (it != sMap.cend())
		return it->second;

	std::shared_ptr<Sound> sound = std::shared_ptr<Sound>(new Sound(file, playbackRate));

	if (AudioManager::isInitialized())
	{
		AudioManager::getInstance()->registerSound(sound);
		sMap[key] = sound;
	}

	return sound;
}

std::shared_ptr<Sound> Sound::getFromTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element)
{
	LOG(LogInfo) << " req sound [" << view << "." << element << "]";

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "sound");
	if (!elem || !elem->has("path"))
	{
		LOG(LogInfo) << "   (missing)";
		return get("");
	}

	return get(elem->get<std::string>("path"));
}

std::shared_ptr<Sound> Sound::getScrollSound(const ThemeData* theme, const std::string& view, float playbackRate)
{
	if (theme != nullptr)
	{
		for (const auto& element : { "gamelist", "gamegrid", "gamecarousel", "systemcarousel" })
		{
			const auto* elem = theme->getElement(view, element, "");
			if (elem != nullptr && elem->has("scrollSound"))
				return get(elem->get<std::string>("scrollSound"), playbackRate);
		}
	}

	return nullptr;
}

Sound::Sound(const std::string & path, float playbackRate) : mSampleData(NULL), mPlayingChannel(-1), mPlaybackRate(playbackRate)
{
	loadFile(path);
}

Sound::~Sound()
{
	deinit();
}

void Sound::loadFile(const std::string & path)
{
	mPath = path;
	init();
}

void Sound::init()
{
	deinit();

	if (!AudioManager::isInitialized())
		return;

	if (mPath.empty() || !Utils::FileSystem::exists(mPath))
		return;

	if (!Settings::getInstance()->getBool("EnableSounds"))
		return;

	//load wav file via SDL
	mSampleData = Mix_LoadWAV(mPath.c_str());
	if (mSampleData == nullptr)
	{
		LOG(LogError) << "Error loading sound \"" << mPath << "\"!\n" << "	" << SDL_GetError();
		return;
	}

	if (mPlaybackRate != 1.0f)
	{
		int frequency, channels;
		Uint16 format;
		SDL_AudioCVT conversion;
		if (Mix_QuerySpec(&frequency, &format, &channels) == 0 ||
			SDL_BuildAudioCVT(&conversion, format, channels, static_cast<int>(frequency * mPlaybackRate),
				format, channels, frequency) < 0)
		{
			LOG(LogError) << "Could not configure sound playback rate: " << SDL_GetError();
			return;
		}

		conversion.len = mSampleData->alen;
		conversion.buf = static_cast<Uint8*>(SDL_malloc(conversion.len * conversion.len_mult));
		if (conversion.buf == nullptr)
			return;
		SDL_memcpy(conversion.buf, mSampleData->abuf, conversion.len);
		if (SDL_ConvertAudio(&conversion) < 0)
		{
			LOG(LogError) << "Could not convert sound playback rate: " << SDL_GetError();
			SDL_free(conversion.buf);
			return;
		}

		auto* sample = Mix_QuickLoad_RAW(conversion.buf, conversion.len_cvt);
		if (sample == nullptr)
		{
			SDL_free(conversion.buf);
			return;
		}
		// Transfer the converted buffer to SDL_mixer for cleanup with the chunk.
		sample->allocated = 1;
		Mix_FreeChunk(mSampleData);
		mSampleData = sample;
	}
}

void Sound::deinit()
{
	if (mSampleData == nullptr)
		return;

	stop();
	Mix_FreeChunk(mSampleData);
	mSampleData = nullptr;	
}

void Sound::play(float volume)
{
	if (mSampleData == nullptr)
		return;

	if (!Settings::getInstance()->getBool("EnableSounds"))
		return;

	// Set volume before playback, and reset it for every use of a mixer channel.
	const int channel = Mix_GroupAvailable(-1);
	if (channel < 0)
		return;
	Mix_Volume(channel, static_cast<int>(std::max(0.0f, std::min(1.0f, volume)) * MIX_MAX_VOLUME + 0.5f));
	mPlayingChannel = Mix_PlayChannel(channel, mSampleData, 0);
}

bool Sound::isPlaying() const
{	
	return (mPlayingChannel >= 0);
}

void Sound::stop()
{
	if (mPlayingChannel < 0)
		return;

	//Mix_HaltChannel(mPlayingChannel);
	mPlayingChannel = -1;
}
