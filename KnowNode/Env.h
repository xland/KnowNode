#pragma once

#include <filesystem>

class Env
{
public:
    static void init();
    static std::filesystem::path getDataPath();

private:
    static void initDataPath();

private:
    static std::filesystem::path dataPath;
};
