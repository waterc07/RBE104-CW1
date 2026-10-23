#include "game.h"
#include "scene.h"
int main(void)
{
    initialise(2,42);
    drawScene(42,0);
    return savePreview("preview.ppm")?0:1;
}
