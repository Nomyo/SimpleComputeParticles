#include <VulkanCore.h>

int main() {

    VulkanCore *core = new VulkanCore(true);
    core->SetupWindow();
    core->InitVulkan();
    core->Prepare();
    core->RenderLoop();
    delete(core);
    return 0;
}