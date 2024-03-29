#include "HmlQueries.h"

#if USE_TIMESTAMP_QUERIES

std::unique_ptr<HmlQueries> HmlQueries::create(std::shared_ptr<HmlDevice> hmlDevice, std::shared_ptr<HmlCommands> hmlCommands, uint32_t depth) noexcept {
    auto hmlQueries = std::make_unique<HmlQueries>();
    hmlQueries->hmlDevice = hmlDevice;
    hmlQueries->hmlCommands = hmlCommands;

    const auto commandBuffer = hmlCommands->beginLongTermSingleTimeCommand();
    for (uint32_t i = 0; i < depth; i++) {
        auto pool = hmlQueries->createPool(QUERIES_IN_POOL);
        if (!pool) return {};
        hmlQueries->pools.push(*pool);
        vkCmdResetQueryPool(commandBuffer, *pool, 0, QUERIES_IN_POOL);
    }
    hmlCommands->endLongTermSingleTimeCommand(commandBuffer);

    hmlQueries->layout = std::make_shared<std::vector<LayoutItem>>();

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(hmlDevice->physicalDevice, &properties);
    if (!properties.limits.timestampComputeAndGraphics) {
        std::cout << "::> The device does not support timestamp queries\n";
        return {};
    }
    const float timestampPeriod = properties.limits.timestampPeriod;
    hmlQueries->timestampPeriod = static_cast<uint64_t>(timestampPeriod);

    return hmlQueries;
}


HmlQueries::~HmlQueries() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlQueries...\n";
#endif

    while (!pools.empty()) {
        vkDestroyQueryPool(hmlDevice->device, pools.front(), nullptr);
        pools.pop();
    }
}


std::optional<VkQueryPool> HmlQueries::createPool(uint32_t count) noexcept {
    VkQueryPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    poolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    poolCreateInfo.queryCount = count;

    const auto device = hmlDevice->device;
    VkQueryPool pool;
    if (const auto result = vkCreateQueryPool(device, &poolCreateInfo, nullptr, &pool);
            result != VK_SUCCESS) {
        std::cout << "::> Unexpected result while creating a pool in HmlQueries: " << result << ".\n";
        return std::nullopt;
    }

    return { pool };
}


std::vector<std::optional<uint64_t>> HmlQueries::query(VkQueryPool pool) noexcept {
    const auto count = usedQueriesInPool[pool];
    assert(count > 0 && "::> Unexpected zero-queries pool in HmlQueries.\n");
    std::vector<uint64_t> data(2 * count); // counter + availability data
    const auto flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
    const auto device = hmlDevice->device;
    const auto sizeBytes = data.size() * sizeof(data[0]);
    const auto stride = 2 * sizeof(data[0]);
    if (const auto result = vkGetQueryPoolResults(device, pool, 0, count, sizeBytes, data.data(), stride, flags);
            !(result == VK_SUCCESS || result == VK_NOT_READY)) {
        std::cout << "::> Unexpected result while trying to query a pool in HmlQueries: " << result << ".\n";
        return {};
    }

    std::vector<std::optional<uint64_t>> result(count, std::nullopt);
    for (size_t i = 0; i < data.size(); i += 2) {
        if (data[i + 1]) result[i / 2] = { data[i] * timestampPeriod };
    }
    return result;
}


// TODO add support for static registrations
void HmlQueries::registerEvent(std::string_view name, std::string_view shortName, VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage) noexcept {
    if (!hasBegun) {
        std::cout << ":> HmlQueries: not registering " << name << " because the frame has not begun yet.\n";
        return;
    }

    assert((layout->size() < QUERIES_IN_POOL) && "::> HmlQueries: while registering a new query: exceeding max allocated pool size for queries.\n");
    if ((layout->size() <= currentEventIndex) || (name != (*layout)[currentEventIndex].name)) {
        replaceLayout();
        layout->emplace_back(std::string(name), std::string(shortName));
    }

    vkCmdWriteTimestamp(commandBuffer, pipelineStage, pools.front(), currentEventIndex);

    currentEventIndex++;
    usedQueriesInPool[pools.front()] = currentEventIndex;
}


std::optional<HmlQueries::FrameStat> HmlQueries::popOldestFrameStat() noexcept {
    if (frameStats.empty()) return std::nullopt;

    const auto el = frameStats.front();
    frameStats.pop();
    return { std::move(el) };
}


void HmlQueries::beginFrame() noexcept {
    hasBegun = true;
    currentEventIndex = 0;
}


void HmlQueries::endFrame(uint32_t currentFrame) noexcept {
    hasBegun = false;

    if (currentEventIndex != layout->size()) {
        replaceLayout();
    }

    pools.push(pools.front()); pools.pop(); // cycle the head
    // Now the head is at the oldest pool
    // Query only when enough frames have passed (when we've completed the full circle)
    const auto queriesCount = usedQueriesInPool[pools.front()];
    if (queriesCount > 0) {
        if (frameStats.size() >= MAX_STORED_FRAME_STAT_COUNT) frameStats.pop();

        frameStats.push(FrameStat{
            .layout = layout,
            .data = query(pools.front()),
        });

        const auto commandBuffer = hmlCommands->beginLongTermSingleTimeCommand();
        vkCmdResetQueryPool(commandBuffer, pools.front(), 0, queriesCount);
        hmlCommands->endLongTermSingleTimeCommand(commandBuffer);
    }
}


void HmlQueries::replaceLayout() noexcept {
    auto copy = std::make_shared<std::vector<LayoutItem>>(*layout);
    layout.swap(copy);
    // Invalidate the rest of the layout, thus making it valid again
    layout->erase(layout->begin() + currentEventIndex, layout->end());
}

#endif // USE_TIMESTAMP_QUERIES
