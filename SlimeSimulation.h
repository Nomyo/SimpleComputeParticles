#pragma once

#include <VulkanCore.h>

#define ENABLE_VALIDATION true
class SlimeSimulation : public VulkanCore
{
public:
    SlimeSimulation();

    virtual void Render();
};

