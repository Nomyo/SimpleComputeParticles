#include <ParticleSimulation.h>

int main() {

    ParticleSimulation *simulation = new ParticleSimulation();
    simulation->SetupWindow();
    simulation->InitVulkan();
    simulation->Prepare();
    simulation->RenderLoop();
    delete(simulation);
    return 0;
}