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

template <size_t size>
std::array<float, size> dot(std::array<float, size> &a,
                            std::array<float, size> &b, int32_t vec_dim) {}

int main() {
  setup_logging();

  VkInstance instance =
      vkc::create_vulkan_instance(VK_MAKE_API_VERSION(1, 3, 236, 0));
  VkPhysicalDevice physical_device = vkc::select_physical_device(instance);

  VkPhysicalDeviceProperties deviceProps;
  vkGetPhysicalDeviceProperties(physical_device, &deviceProps);

  uint32_t maxWorkGroupCountX = deviceProps.limits.maxComputeWorkGroupCount[0];
  uint32_t maxWorkGroupCountY = deviceProps.limits.maxComputeWorkGroupCount[1];
  uint32_t maxWorkGroupCountZ = deviceProps.limits.maxComputeWorkGroupCount[2];
  spdlog::info("Max work group count: {}, {}, {}", maxWorkGroupCountX,
               maxWorkGroupCountY, maxWorkGroupCountZ);

  uint32_t qfidx = vkc::find_queue_family(physical_device);
  VkDevice device = vkc::create_logical_device(physical_device, qfidx);

  constexpr size_t size = 1024;
  const size_t batch_size = 64;
  const size_t vec_dim = size / batch_size;
  spdlog::info("Vector size: {}", vec_dim);
  std::array<float, size> input_a{};
  std::array<float, size> input_b{};
  std::array<float, batch_size> output{};
  for (size_t i = 0; i < size; i++) {
    input_a[i] = static_cast<float>(i);
    // input_b[i] = static_cast<float>(size - i);
    input_b[i] = 1.0f;
  }

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
  vkc::copy_to_gpu<size>(device, buffers.memory[0], input_a);
  vkc::copy_to_gpu<size>(device, buffers.memory[1], input_b);
  VkPipelineLayout pipeline_layout =
      vkc::create_pipeline_layout<n_bindings>(device);
  VkDescriptorSet descriptor_set =
      vkc::create_descriptor_sets<n_bindings>(device, buffers);
  spdlog::info("Created descriptor set.");

  // uint32_t wgsize = static_cast<uint32_t>(output.size());
  uint32_t wgsize = vec_dim;
  std::array<uint32_t, 3> workgroup_size = {wgsize, 1, 1};
  VkShaderModule shader = vkc::create_shader_module(device, "build/dot.spv");
  VkPipeline pipeline =
      vkc::create_pipeline(device, pipeline_layout, shader, workgroup_size);

  VkCommandPool command_pool = vkc::create_command_pool(device, qfidx);
  VkCommandBuffer command_buffer =
      vkc::create_command_buffer(device, command_pool);

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags =
      VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT; // Allow command buffer
                                                    // to be executed
                                                    // multiple times
  VkResult result = vkBeginCommandBuffer(command_buffer, &beginInfo);
  vkc::check(result, "Begin command buffer.");

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
  spdlog::info("Dispatching N workgroups: {}", size / workgroup_size[0]);
  vkCmdDispatch(command_buffer, size / workgroup_size[0], 1, 1);

  result = vkEndCommandBuffer(command_buffer);
  vkc::check(result, "End command buffer.");

  VkQueue queue;
  const uint32_t queue_index = 0;
  vkGetDeviceQueue(device, qfidx, queue_index, &queue);

  std::string input = "";
  while (input != "q") {
    const VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkc::check(result, "Submit command buffer.");

    result = vkQueueWaitIdle(queue);
    vkc::check(result, "Wait for queue to become idle.");

    // Print input and output to the screen
    vkc::copy_to_cpu<batch_size>(device, buffers.memory[2], output);
    int idx = 0;
    /*
    spdlog::info("Input A: ");
    idx = 0;
    for (auto &x : input_a) {
      spdlog::info("{} : {}", idx, x);
      idx++;
    }
    spdlog::info("Input B: ");
    idx = 0;
    idx = 0;
    for (auto &x : input_b) {
      spdlog::info("{} : {}", idx, x);
      idx++;
    }
    */
    spdlog::info("Output: ");
    idx = 0;
    for (auto &x : output) {
      spdlog::info("{} : {}", idx, x);
      idx++;
    }

    // Test re-using the computation or let the user quit
    // std::cout << "Enter q to quit, anything else to re-run computation > ";
    // std::getline(std::cin, input);
    input = "q";
  }

  spdlog::info("Done.");
}
