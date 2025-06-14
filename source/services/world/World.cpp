#include "World.h"

namespace parus
{
    
    void World::tick(const float deltaTime)
    {
        mainCamera.updateTransform(deltaTime);
    }
    
}
