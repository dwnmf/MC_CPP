#include "audio.h"
#include <iostream>
#include <filesystem>
#include <random>
#include <algorithm>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

ma_engine engine;
ma_sound music_sound;
std::vector<std::string> Audio::music_files;
float Audio::next_music_time = 0.0f;
bool Audio::is_playing = false;
float Audio::volume = 0.5f;
bool engine_initialized = false;

void Audio::Init() {
    ma_result result;
    ma_engine_config config = ma_engine_config_init();
    // Можно явно указать громкость движка
    // config.volume = 1.0f;

    result = ma_engine_init(&config, &engine);
    if (result != MA_SUCCESS) {
        std::cout << "[Audio] CRITICAL ERROR: Failed to initialize audio engine. Error code: " << result << std::endl;
        return;
    }

    engine_initialized = true;
    ma_engine_set_volume(&engine, volume);
    std::cout << "[Audio] Engine initialized successfully." << std::endl;

    // Сканируем папку с музыкой
    std::string path = "assets/audio/music/";
    if (std::filesystem::exists(path)) {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            auto ext = entry.path().extension();
            if (ext == ".ogg" || ext == ".mp3" || ext == ".wav") {
                music_files.push_back(entry.path().string());
            }
        }
    } else {
        std::cout << "[Audio] Warning: Music directory not found: " << path << std::endl;
    }

    if (music_files.empty()) {
        std::cout << "[Audio] No music files found." << std::endl;
    } else {
        PlayMusic();
    }
}

void Audio::PlayMusic() {
    if (!engine_initialized || music_files.empty()) return;

    // Очистка предыдущего звука
    if (is_playing) {
        ma_sound_stop(&music_sound); // Сначала стоп
        ma_sound_uninit(&music_sound);
        is_playing = false;
    }

    // Выбор трека
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, music_files.size() - 1);
    std::string track = music_files[distr(gen)];

    std::cout << "[Audio] Attempting to play: " << track << std::endl;

    // ВАЖНО: MA_SOUND_FLAG_STREAM для музыки
    // Это позволяет играть большие файлы, не загружая их целиком в RAM
    ma_result result = ma_sound_init_from_file(&engine, track.c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &music_sound);

    if (result != MA_SUCCESS) {
        std::cout << "[Audio] Failed to load sound file! Error code: " << result << std::endl;
        // Пробуем следующий через 5 секунд, чтобы не спамить ошибками
        next_music_time = 5.0f;
        is_playing = true; // Ставим фейковый флаг, чтобы Update сработал и попробовал снова
        return;
    }

    // Настройка и запуск
    ma_sound_set_volume(&music_sound, volume);
    ma_sound_start(&music_sound);
    is_playing = true;

    // Рассчитываем время до следующего трека
    float length = 0;
    ma_sound_get_length_in_seconds(&music_sound, &length);

    // Если длина не определилась (бывает с потоками), берем дефолт 3 минуты
    if (length <= 0) length = 180.0f;

    // Пауза между треками (от 10 сек до 3 минут)
    float delay = 10.0f + (rand() % 180);
    next_music_time = length + delay;

    std::cout << "[Audio] Playing. Duration: " << length << "s. Next track in: " << (length + delay) << "s." << std::endl;
}

void Audio::Update(float dt) {
    if (is_playing) {
        next_music_time -= dt;

        // Дополнительная проверка: если звук реально перестал играть раньше времени
        if (engine_initialized && !ma_sound_is_playing(&music_sound) && next_music_time > 180.0f) {
            // Если звук кончился, а ждать еще долго (значит пауза еще не наступила, просто трек доиграл)
            // Можно ускорить таймер, оставив только паузу
            // Но пока просто ждем таймера
        }

        if (next_music_time <= 0) {
            PlayMusic();
        }
    }
}

void Audio::PlayPop() {
    if (!engine_initialized) return;
    std::string path = "assets/audio/pop.wav";
    if (!std::filesystem::exists(path)) return;
    ma_result result = ma_engine_play_sound(&engine, path.c_str(), NULL);
    if (result != MA_SUCCESS) {
        std::cout << "[Audio] Failed to play pop sound. Error code: " << result << std::endl;
    }
}

void Audio::Close() {
    if (is_playing) ma_sound_uninit(&music_sound);
    if (engine_initialized) ma_engine_uninit(&engine);
}

void Audio::SetVolume(float newVolume) {
    volume = std::clamp(newVolume, 0.0f, 1.0f);
    if (engine_initialized) {
        ma_engine_set_volume(&engine, volume);
        if (is_playing) {
            ma_sound_set_volume(&music_sound, volume);
        }
    }
}

float Audio::GetVolume() {
    return volume;
}
