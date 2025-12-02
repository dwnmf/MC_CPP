#pragma once
#include <string>

class World;

class Save {
public:
    World* world;
    std::string path;
    Save(World* w);
    void save();
    void load();
};
