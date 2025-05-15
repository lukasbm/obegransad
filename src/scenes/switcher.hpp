#include <Arduino.h>
#include <array>
#include "scene.h"
#include "led.h"

template <size_t numScenes>
class SceneSwitcher
{
public:
    // constexpr SceneSwitcher(std::initializer_list<Scene *> sceneList) : scenes(sceneList), currIdx(-1)  // idk if this adds too much flash size
    constexpr SceneSwitcher(std::array<Scene *, numScenes> scenes) : scenes(scenes), currIdx(-1)
    {
    }

    ~SceneSwitcher()
    {
        scenes[currIdx]->deactivate();
    }

    void nextScene()
    {
        if (currIdx >= 0) // edge case at startup
        {
            scenes[currIdx]->deactivate();
        }
        currIdx = (currIdx + 1) % numScenes;
        panel_clear();
        scenes[currIdx]->activate();
    }

    void prevScene()
    {
        if (currIdx >= 0) // edge case at startup
        {
            scenes[currIdx]->deactivate();
        }
        currIdx = (currIdx + numScenes - 1) % numScenes;
        panel_clear();
        scenes[currIdx]->activate();
    }

    void tick()
    {
        scenes[currIdx]->update();
    }

private:
    // pointers for polymorphic dispatch
    const std::array<Scene *, numScenes> scenes;
    short currIdx;
};
