#ifndef HML_QUERIES
#define HML_QUERIES

#include <string_view>
#include <unordered_map>
#include <queue>
#include <memory>
#include <optional>

#include "settings.h"
#include "HmlDevice.h"
#include "HmlCommands.h"


struct HmlQueries {
    struct FrameStat {
        std::shared_ptr<std::vector<std::string>> layout;
        std::vector<std::optional<uint64_t>> data;
    };

    static std::unique_ptr<HmlQueries> create(std::shared_ptr<HmlDevice> hmlDevice, std::shared_ptr<HmlCommands> hmlCommands, uint32_t depth) noexcept;
    ~HmlQueries() noexcept;
    std::optional<VkQueryPool> createPool(uint32_t count) noexcept;
    std::vector<std::optional<uint64_t>> query(VkQueryPool pool) noexcept;
    void registerEvent(std::string_view name, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage) noexcept;
    std::optional<FrameStat> popOldestFrameStat() noexcept;
    void beginFrame() noexcept;
    void endFrame(uint32_t currentFrame) noexcept;
    void replaceLayout() noexcept;


    static constexpr uint32_t QUERIES_IN_POOL = 20;
    static constexpr uint32_t MAX_STORED_FRAME_STAT_COUNT = 32;

    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;

    uint64_t timestampPeriod;
    bool hasBegun = false;

    uint32_t currentEventIndex;
    std::shared_ptr<std::vector<std::string>> layout;
    std::queue<FrameStat> frameStats;
    std::queue<VkQueryPool> pools;
    std::unordered_map<VkQueryPool, uint32_t> usedQueriesInPool;
};


#endif
