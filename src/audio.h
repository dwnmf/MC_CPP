#pragma once
#include <vector>
#include <string>

class Audio {
public:
    static void Init();
    static void Update(float dt);
    static void Close();
    static void PlayMusic();

private:
    static std::vector<std::string> music_files;
    static float next_music_time;
    static bool is_playing;
};
