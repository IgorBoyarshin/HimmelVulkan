#include <memory> // shaded_ptr
#include <vector>

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
#include "HmlResourceManager.h"
#include "HmlRenderer.h"
#include "HmlModel.h"


struct Himmel {
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlRenderer> hmlRenderer;


    bool init() {
        const char* windowName = "Planes game";

        hmlWindow = HmlWindow::create(1920, 1080, windowName);
        if (!hmlWindow) return false;

        hmlDevice = HmlDevice::create(hmlWindow);
        if (!hmlDevice) return false;

        hmlCommands = HmlCommands::create(hmlDevice);
        if (!hmlCommands) return false;

        hmlResourceManager = HmlResourceManager::create(hmlDevice, hmlCommands);
        if (!hmlResourceManager) return false;

        hmlSwapchain = HmlSwapchain::create(hmlWindow, hmlDevice, hmlResourceManager);
        if (!hmlSwapchain) return false;

        hmlRenderer = HmlRenderer::createSimpleRenderer(hmlWindow, hmlDevice, hmlCommands, hmlSwapchain, hmlResourceManager);
        if (!hmlRenderer) return false;


        {
            std::vector<HmlSimpleModel::Vertex> vertices;
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, -0.5f, 0.0f},
                .color = {1.0f, 0.0f, 0.0f},
                .texCoord = {1.0f, 0.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, -0.5f, 0.0f},
                .color = {0.0f, 1.0f, 0.0f},
                .texCoord = {0.0f, 0.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, 0.5f, 0.0f},
                .color = {0.0f, 0.0f, 1.0f},
                .texCoord = {0.0f, 1.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, 0.5f, 0.0f},
                .color = {1.0f, 1.0f, 1.0f},
                .texCoord = {1.0f, 1.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, -0.5f, -1.0f},
                .color = {1.0f, 0.0f, 0.0f},
                .texCoord = {1.0f, 0.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, -0.5f, -1.0f},
                .color = {0.0f, 1.0f, 0.0f},
                .texCoord = {0.0f, 0.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, 0.5f, -1.0f},
                .color = {0.0f, 0.0f, 1.0f},
                .texCoord = {0.0f, 1.0f}
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, 0.5f, -1.0f},
                .color = {1.0f, 1.0f, 1.0f},
                .texCoord = {1.0f, 1.0f}
            });

            const std::vector<uint16_t> indices = {
                0, 1, 2, 2, 3, 0,
                4, 5, 6, 6, 7, 4,
            };

            hmlRenderer->addEntity(vertices.data(), vertices.size() * sizeof(HmlSimpleModel::Vertex), indices);

            hmlRenderer->bakeCommandBuffers();
        }


        hmlRenderer->run();


        return true;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
};
