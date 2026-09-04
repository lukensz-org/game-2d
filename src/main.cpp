#include <engine/graphics/vulkan/graphics.hpp>
#include <engine/platform/window.hpp>

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

using engine::graphics::vulkan::FrameBeginDisposition;
using engine::graphics::vulkan::FrameBeginResult;
using engine::graphics::vulkan::FrameEndDisposition;
using engine::graphics::vulkan::GraphicsContext;
using engine::graphics::vulkan::rendering::ColorPass;
using engine::graphics::vulkan::rendering::DrawInfo;
using engine::graphics::vulkan::rendering::GraphicsState;
using engine::graphics::vulkan::rendering::PushConstantRange;
using engine::graphics::vulkan::rendering::PushConstantWrite;

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

GraphicsState makeGraphicsState(const GraphicsContext &graphics,
                                std::span<const std::uint32_t> vertex,
                                std::span<const std::uint32_t> fragment, VkFormat color_format) {
    constexpr std::array push_ranges{
        PushConstantRange{.stages = VK_SHADER_STAGE_VERTEX_BIT,
                          .offset = 0,
                          .size = static_cast<std::uint32_t>(sizeof(SquarePush))},
    };
    return graphics.createGraphicsState({
        .vertex_spirv = vertex,
        .fragment_spirv = fragment,
        .push_constant_ranges = push_ranges,
        .color_format = color_format,
    });
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
        engine::platform::WindowConfig config;
        config.title = "game-2d";
        config.visible = !options.bounded;
        const engine::platform::Window window{window_system, config};
        GraphicsContext graphics{window_system, window};
        std::optional<GraphicsState> graphics_state;
        std::optional<VkFormat> graphics_state_color_format;

        std::uint32_t rendered_frames = 0;
        std::uint32_t bounded_iterations = 0;
        constexpr std::uint32_t bounded_frame_target = 4;
        constexpr std::uint32_t bounded_iteration_limit = 512;

        const auto applyReadyGeneration = [&](const FrameBeginResult &frame) {
            if (frame.disposition != FrameBeginDisposition::ready) {
                throw std::logic_error("Generation state requires a ready frame.");
            }
            if (frame.extent.width == 0U || frame.extent.height == 0U ||
                frame.color_format == VK_FORMAT_UNDEFINED) {
                throw std::logic_error("Ready frame reported invalid presentation state.");
            }

            if (!graphics_state.has_value() || !graphics_state_color_format.has_value() ||
                *graphics_state_color_format != frame.color_format) {
                if (graphics_state.has_value()) {
                    graphics.waitForSubmittedWork();
                }
                graphics_state.reset();
                graphics_state =
                    makeGraphicsState(graphics, vertex_spirv, fragment_spirv, frame.color_format);
                graphics_state_color_format = frame.color_format;
            }

            if (frame.generation_changed) {
                std::cout << "Swapchain generation active: " << frame.generation
                          << " extent=" << frame.extent.width << 'x' << frame.extent.height << '\n';
            }
        };

        const std::uint32_t api_version = graphics.applicationApiVersion();
        std::cout << "game-2d initialized: " << graphics.deviceName() << " (API "
                  << VK_API_VERSION_MAJOR(api_version) << '.' << VK_API_VERSION_MINOR(api_version)
                  << ")\n";
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
            const auto frame = graphics.beginFrame(window.framebufferExtent());
            for (const std::uint64_t generation : frame.reclaimed_generations) {
                std::cout << "Retired swapchain generation destroyed: " << generation << '\n';
            }

            switch (frame.disposition) {
            case FrameBeginDisposition::deferred:
                std::this_thread::sleep_for(std::chrono::milliseconds{8});
                continue;
            case FrameBeginDisposition::try_again:
                continue;
            case FrameBeginDisposition::surface_lost:
                throw std::runtime_error("Presentation surface was lost while beginning a frame.");
            case FrameBeginDisposition::ready:
                break;
            }

            applyReadyGeneration(frame);
            if (!graphics_state.has_value()) {
                throw std::logic_error("Ready frame has no compatible graphics state.");
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

            const auto presented = graphics.presentFrame(pass);
            switch (presented.disposition) {
            case FrameEndDisposition::surface_lost:
                throw std::runtime_error(
                    "Presentation surface was lost during queue presentation.");
            case FrameEndDisposition::try_again:
                continue;
            case FrameEndDisposition::accepted:
                break;
            }

            ++rendered_frames;
            if (options.bounded) {
                std::cout << "frame=" << rendered_frames << " generation=" << frame.generation
                          << " image=" << frame.image_index
                          << " completion=" << presented.completion.value << '\n';
            }
        }

        graphics.waitForSubmittedWork();

        std::cout << "game_2d=passed frames=" << rendered_frames
                  << " squares=" << kSquares.size()
                  << (options.bounded ? " mode=bounded\n" : " mode=interactive\n");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "game_2d=failed: " << error.what() << '\n';
        return 1;
    }
}
