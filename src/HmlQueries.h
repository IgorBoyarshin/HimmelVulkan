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
    struct LayoutItem {
        std::string name;
        std::string shortName;
        inline LayoutItem(std::string&& name, std::string&& shortName) : name(std::move(name)), shortName(std::move(shortName)) {}
    };

    struct FrameStat {
        std::shared_ptr<std::vector<LayoutItem>> layout;
        std::vector<std::optional<uint64_t>> data;
    };

    static std::unique_ptr<HmlQueries> create(std::shared_ptr<HmlDevice> hmlDevice, std::shared_ptr<HmlCommands> hmlCommands, uint32_t depth) noexcept;
    ~HmlQueries() noexcept;
    std::optional<VkQueryPool> createPool(uint32_t count) noexcept;
    std::vector<std::optional<uint64_t>> query(VkQueryPool pool) noexcept;
    void registerEvent(std::string_view name, std::string_view shortName, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage) noexcept;
    std::optional<FrameStat> popOldestFrameStat() noexcept;
    void beginFrame() noexcept;
    void endFrame(uint32_t currentFrame) noexcept;
    void replaceLayout() noexcept;


    static constexpr uint32_t QUERIES_IN_POOL = 32;
    static constexpr uint32_t MAX_STORED_FRAME_STAT_COUNT = 32;

    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;

    uint64_t timestampPeriod;
    bool hasBegun = false;

    uint32_t currentEventIndex;
    std::shared_ptr<std::vector<LayoutItem>> layout;
    std::queue<FrameStat> frameStats;
    std::queue<VkQueryPool> pools;
    std::unordered_map<VkQueryPool, uint32_t> usedQueriesInPool;
};


#endif
