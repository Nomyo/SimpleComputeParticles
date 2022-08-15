#include <VulkanCore.h>

int main() {

    VulkanCore *core = new VulkanCore(true);
    core->InitVulkan();
    core->SetupWindow();
    core->Prepare();
    core->RenderLoop();
    delete(core);
    return 0;
}