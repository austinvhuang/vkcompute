#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_macos.h>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "vkcompute.hpp"

void setup_logging() {
  spdlog::set_level(spdlog::level::trace);
  spdlog::set_pattern("[%^%l%$] [%H:%M:%S] %v");
  auto file_logger =
      spdlog::basic_logger_mt("basic_logger", "logs/experimental_log.txt");
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::flush_every(std::chrono::seconds(3));
}

int main() {
  setup_logging();

  VkInstance instance =
      vkc::create_vulkan_instance(VK_MAKE_API_VERSION(1, 3, 236, 0));
  VkPhysicalDevice physical_device = vkc::select_physical_device(instance);
  uint32_t qfidx = vkc::find_queue_family(physical_device);
  VkDevice device = vkc::create_logical_device(physical_device, qfidx);

  constexpr size_t size = 8;
  std::array<float, size> input_a{};
  std::array<float, size> input_b{};
  for (size_t i = 0; i < size; i++) {
    input_a[i] = static_cast<float>(i);
    input_b[i] = static_cast<float>(size - i);
  }
  std::array<float, size> output{};

  constexpr size_t n_bindings = 3;
  auto memory_type = vkc::query_memory_type(physical_device);
  if (!memory_type) {
    spdlog::error("Failed to find memory type");
    std::runtime_error("Failed to find memory type");
  }
  vkc::GPUBuffers<n_bindings> buffers{memory_type.value()};

  vkc::gpu_alloc<n_bindings>(device, input_a.size(),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, buffers);
  vkc::gpu_alloc<n_bindings>(device, input_b.size(),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, buffers);
  vkc::gpu_alloc<n_bindings>(device, output.size(),
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             buffers);
  vkc::copy_to_gpu<size>(device, buffers.memories[0], input_a);
  vkc::copy_to_gpu<size>(device, buffers.memories[1], input_b);
  VkPipelineLayout pipeline_layout =
      vkc::create_pipeline_layout<n_bindings>(device);
  vkc::create_descriptor_sets<n_bindings>(device, buffers);
  spdlog::info("Created descriptor set.");

  uint32_t wgsize = static_cast<uint32_t>(output.size());
  std::array<uint32_t, 3> workgroup_size = {wgsize, 1, 1};
  VkShaderModule shader = vkc::create_shader_module(device, "build/dot.spv");
  VkPipeline pipeline =
      vkc::create_pipeline(device, pipeline_layout, shader, workgroup_size);

  VkCommandPool command_pool = vkc::create_command_pool(device, qfidx);
  VkCommandBuffer command_buffer =
      vkc::create_command_buffer(device, command_pool);

  spdlog::info("Done.");
}
