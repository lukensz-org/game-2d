#include <engine/graphics/vulkan/runtime.hpp>
#include <engine/graphics/vulkan/swapchain_lifecycle.hpp>
#include <engine/graphics/vulkan/wsi.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifndef GAME_2D_VERTEX_SPV
#error "GAME_2D_VERTEX_SPV must name the generated vertex SPIR-V file."
#endif

#ifndef GAME_2D_FRAGMENT_SPV
#error "GAME_2D_FRAGMENT_SPV must name the generated fragment SPIR-V file."
#endif

namespace {

using engine::graphics::vulkan::presentation::PixelExtent;
using engine::graphics::vulkan::rendering::ColorPass;
using engine::graphics::vulkan::rendering::DrawInfo;
using engine::graphics::vulkan::rendering::FrameAcquireDisposition;
using engine::graphics::vulkan::rendering::FramePresentDisposition;
using engine::graphics::vulkan::rendering::GraphicsState;
using engine::graphics::vulkan::rendering::PushConstantRange;
using engine::graphics::vulkan::rendering::PushConstantWrite;
using engine::graphics::vulkan::rendering::RenderingContext;
using engine::graphics::vulkan::rendering::SwapchainLifecycle;

struct RunOptions final {
    bool bounded = false;
};

struct SquarePush final {
    float center_x = 0.0F;
    float center_y = 0.0F;
    float half_extent_x = 0.0F;
    float half_extent_y = 0.0F;
    float color_r = 0.0F;
    float color_g = 0.0F;
    float color_b = 0.0F;
    float color_a = 1.0F;
};

static_assert(sizeof(SquarePush) == 32U);

class ShutdownIdleGuard final {
  public:
    explicit ShutdownIdleGuard(VkDevice device) : device_(device) {}
    ~ShutdownIdleGuard() {
        if (device_ != VK_NULL_HANDLE) {
            static_cast<void>(vkDeviceWaitIdle(device_));
        }
    }

    ShutdownIdleGuard(const ShutdownIdleGuard &) = delete;
    ShutdownIdleGuard &operator=(const ShutdownIdleGuard &) = delete;

  private:
    VkDevice device_ = VK_NULL_HANDLE;
};

RunOptions parseOptions(int argc, char **argv) {
    RunOptions options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--bounded") {
            options.bounded = true;
            continue;
        }
        throw std::invalid_argument("Unknown application argument: " + std::string(argument));
    }
    return options;
}

std::vector<std::uint32_t> loadSpirv(const char *path) {
    std::ifstream file{path, std::ios::binary | std::ios::ate};
    if (!file) {
        throw std::runtime_error(std::string("Could not open generated SPIR-V: ") + path);
    }
    const std::streamsize byte_size = file.tellg();
    if (byte_size <= 0 || (byte_size % static_cast<std::streamsize>(sizeof(std::uint32_t))) != 0) {
        throw std::runtime_error(std::string("Generated SPIR-V has invalid byte size: ") + path);
    }
    file.seekg(0);
    std::vector<std::uint32_t> words(static_cast<std::size_t>(byte_size) / sizeof(std::uint32_t));
    if (!file.read(reinterpret_cast<char *>(words.data()), byte_size)) {
        throw std::runtime_error(std::string("Could not read generated SPIR-V: ") + path);
    }
    return words;
}

PixelExtent toPixelExtent(engine::platform::FramebufferExtent extent) {
    if (extent.width <= 0 || extent.height <= 0) {
        return {};
    }
    return {.width = static_cast<std::uint32_t>(extent.width),
            .height = static_cast<std::uint32_t>(extent.height)};
}

GraphicsState makeGraphicsState(RenderingContext &rendering, std::span<const std::uint32_t> vertex,
                                std::span<const std::uint32_t> fragment, VkFormat color_format) {
    constexpr std::array push_ranges{
        PushConstantRange{.stages = VK_SHADER_STAGE_VERTEX_BIT,
                          .offset = 0,
                          .size = static_cast<std::uint32_t>(sizeof(SquarePush))},
    };
    return rendering.createGraphicsState({
        .vertex_spirv = vertex,
        .fragment_spirv = fragment,
        .push_constant_ranges = push_ranges,
        .color_format = color_format,
    });
}

void requireShutdownIdle(VkDevice device) {
    const VkResult result = vkDeviceWaitIdle(device);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("vkDeviceWaitIdle(shutdown) failed with VkResult " +
                                 std::to_string(static_cast<int>(result)));
    }
}

constexpr std::array kSquares{
    SquarePush{-0.55F, 0.35F, 0.18F, 0.18F, 0.92F, 0.28F, 0.28F, 1.0F},
    SquarePush{0.05F, 0.20F, 0.22F, 0.22F, 0.28F, 0.78F, 0.42F, 1.0F},
    SquarePush{0.50F, -0.10F, 0.16F, 0.16F, 0.28F, 0.52F, 0.95F, 1.0F},
    SquarePush{-0.25F, -0.40F, 0.20F, 0.20F, 0.95F, 0.78F, 0.22F, 1.0F},
};

} // namespace

int main(int argc, char **argv) {
    try {
        const RunOptions options = parseOptions(argc, argv);
        const auto vertex_spirv = loadSpirv(GAME_2D_VERTEX_SPV);
        const auto fragment_spirv = loadSpirv(GAME_2D_FRAGMENT_SPV);

        engine::platform::WindowSystem window_system;
        const auto wsi_requirements =
            engine::graphics::vulkan::wsi::queryInstanceRequirements(window_system);
        const auto presentation_requirements =
            engine::graphics::vulkan::presentation::queryInstanceRequirements();
        const auto wsi_views = wsi_requirements.views();
        const auto presentation_views = presentation_requirements.views();
        std::vector<std::string_view> required_extensions;
        required_extensions.reserve(wsi_views.size() + presentation_views.size());
        required_extensions.insert(required_extensions.end(), wsi_views.begin(), wsi_views.end());
        required_extensions.insert(required_extensions.end(), presentation_views.begin(),
                                   presentation_views.end());
        const engine::graphics::vulkan::Runtime runtime{
            {.required_instance_extensions =
                 std::span<const std::string_view>{required_extensions}}};

        engine::platform::WindowConfig config;
        config.title = "game-2d";
        config.visible = !options.bounded;
        const engine::platform::Window window{window_system, config};
        const engine::graphics::vulkan::wsi::Surface surface{runtime.instance(), window};
        const auto selected = engine::graphics::vulkan::device::selectPhysicalDevice(
            runtime.instance(), surface.handle(), runtime.applicationApiVersion());
        const engine::graphics::vulkan::logical_device::LogicalDevice logical_device{selected};
        engine::graphics::vulkan::resources::ResourceAllocator allocator{runtime.instance(),
                                                                         selected, logical_device};
        engine::graphics::vulkan::execution::GraphicsExecutionContext execution{
            logical_device.handle(), logical_device.queues().graphics};
        engine::graphics::vulkan::presentation::Swapchain swapchain{
            selected.capabilities.handle, logical_device.handle(), surface.handle(),
            logical_device.queues()};
        RenderingContext rendering{selected, logical_device, allocator, execution};
        SwapchainLifecycle lifecycle{swapchain, rendering};
        std::optional<GraphicsState> graphics_state;

        std::uint32_t rendered_frames = 0;
        std::uint32_t bounded_iterations = 0;
        constexpr std::uint32_t bounded_frame_target = 4;
        constexpr std::uint32_t bounded_iteration_limit = 512;

        const auto applyCreatedGeneration = [&]() {
            const auto &active = lifecycle.swapchain().activeGeneration();
            graphics_state = makeGraphicsState(rendering, vertex_spirv, fragment_spirv,
                                               active.config.surface_format.format);
            std::cout << "Swapchain generation active: " << active.id
                      << " extent=" << active.config.extent.width << 'x'
                      << active.config.extent.height << " images=" << active.images.size() << '\n';
        };

        const PixelExtent initial_extent = toPixelExtent(window.framebufferExtent());
        if (initial_extent.width != 0U && initial_extent.height != 0U) {
            const auto outcome = lifecycle.sync(initial_extent);
            if (outcome.has_value() &&
                outcome->status ==
                    engine::graphics::vulkan::presentation::RecreateStatus::surface_lost) {
                throw std::runtime_error("Presentation surface was lost during recreation.");
            }
            if (outcome.has_value() &&
                outcome->status ==
                    engine::graphics::vulkan::presentation::RecreateStatus::created) {
                applyCreatedGeneration();
            }
        }

        const ShutdownIdleGuard shutdown_idle{logical_device.handle()};
        static_cast<void>(shutdown_idle);

        std::cout << "game-2d initialized: " << selected.capabilities.name << " (API "
                  << VK_API_VERSION_MAJOR(selected.capabilities.effective_api_version) << '.'
                  << VK_API_VERSION_MINOR(selected.capabilities.effective_api_version)
                  << ") validation=" << (runtime.validationEnabled() ? "enabled" : "disabled")
                  << '\n';
        std::cout << "squares=" << kSquares.size() << '\n';

        while (!window.shouldClose()) {
            if (options.bounded && rendered_frames >= bounded_frame_target) {
                break;
            }
            if (options.bounded && ++bounded_iterations > bounded_iteration_limit) {
                throw std::runtime_error("Bounded rendering did not complete its presentation "
                                         "target deterministically.");
            }

            window_system.pollEvents();
            const PixelExtent framebuffer_extent = toPixelExtent(window.framebufferExtent());
            if (framebuffer_extent.width == 0U || framebuffer_extent.height == 0U) {
                std::this_thread::sleep_for(std::chrono::milliseconds{8});
                continue;
            }

            if (const auto outcome = lifecycle.sync(framebuffer_extent); outcome.has_value()) {
                if (outcome->status ==
                    engine::graphics::vulkan::presentation::RecreateStatus::surface_lost) {
                    throw std::runtime_error("Presentation surface was lost during recreation.");
                }
                if (outcome->status ==
                    engine::graphics::vulkan::presentation::RecreateStatus::deferred) {
                    std::this_thread::sleep_for(std::chrono::milliseconds{8});
                    continue;
                }
                if (outcome->status ==
                    engine::graphics::vulkan::presentation::RecreateStatus::created) {
                    applyCreatedGeneration();
                }
            }

            for (const std::uint64_t generation : lifecycle.reclaimRetiredGenerations()) {
                std::cout << "Retired swapchain generation destroyed: " << generation << '\n';
            }

            const auto acquired = lifecycle.acquire();
            if (acquired.disposition == FrameAcquireDisposition::try_again) {
                continue;
            }
            if (acquired.disposition == FrameAcquireDisposition::recreate_required) {
                continue;
            }
            if (acquired.disposition == FrameAcquireDisposition::surface_lost) {
                throw std::runtime_error("Presentation surface was lost during image acquisition.");
            }
            if (!acquired.image.has_value() || !graphics_state.has_value()) {
                throw std::logic_error("Rendering acquire succeeded without frame state.");
            }

            std::array<PushConstantWrite, kSquares.size()> push_writes{};
            std::array<DrawInfo, kSquares.size()> draws{};
            for (std::size_t index = 0; index < kSquares.size(); ++index) {
                push_writes[index] = PushConstantWrite{
                    .stages = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .data = std::as_bytes(std::span{&kSquares[index], 1}),
                };
                draws[index] = DrawInfo{
                    .graphics_state = &*graphics_state,
                    .push_constants = std::span<const PushConstantWrite>{&push_writes[index], 1},
                    .vertex_count = 6,
                    .instance_count = 1,
                };
            }

            VkClearColorValue clear{};
            clear.float32[0] = 0.04F;
            clear.float32[1] = 0.05F;
            clear.float32[2] = 0.09F;
            clear.float32[3] = 1.0F;
            const ColorPass pass{
                .draws = draws,
                .load_op = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .store_op = VK_ATTACHMENT_STORE_OP_STORE,
                .clear_color = clear,
            };

            const auto frame = lifecycle.renderAndPresent(*acquired.image, pass);
            ++rendered_frames;
            if (frame.disposition == FramePresentDisposition::surface_lost) {
                throw std::runtime_error(
                    "Presentation surface was lost during queue presentation.");
            }
            if (options.bounded) {
                std::cout << "frame=" << rendered_frames
                          << " generation=" << acquired.image->generation
                          << " image=" << acquired.image->image_index
                          << " completion=" << frame.frame.completion.value << '\n';
            }
        }

        rendering.waitForSubmittedWork();
        for (const std::uint64_t generation : lifecycle.reclaimRetiredGenerations()) {
            std::cout << "Retired swapchain generation destroyed: " << generation << '\n';
        }
        requireShutdownIdle(logical_device.handle());

        std::cout << "game_2d=passed frames=" << rendered_frames
                  << " squares=" << kSquares.size()
                  << (options.bounded ? " mode=bounded\n" : " mode=interactive\n");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "game_2d=failed: " << error.what() << '\n';
        return 1;
    }
}
