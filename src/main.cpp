
#define VK_ENABLE_BETA_EXTENSIONS // VK_KHR_portability_subset
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_macos.h>

#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#include "vkcompute.hpp"

void setup_logging() {
  spdlog::set_level(spdlog::level::trace);
  spdlog::set_pattern("[%H:%M:%S %z] [%^%l%$] [thread %t] %v");
  auto file_logger =
      spdlog::basic_logger_mt("basic_logger", "logs/vulkan_log.txt");
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::flush_every(std::chrono::seconds(3));
  spdlog::info("spdlogging configured");
}

int main() {
  setup_logging();

  constexpr int n_layers = 1;
  std::array<char *, n_layers> instance_layer_names = {
      const_cast<char *>("VK_LAYER_KHRONOS_validation")};

  VkInstance instance = mk_vulkan_instance<n_layers>(instance_layer_names);
  VkPhysicalDevice physical_device = mk_physcal_device(instance);
  uint32_t qfidx = find_queue_family(physical_device);
  constexpr int n_device_extensions = 1;
  // add portability and printf
  std::array<char *, 1> extension_names = {
      const_cast<char *>(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)};
  VkDevice device =
      mk_logical_device<1>(physical_device, qfidx, extension_names);

  constexpr int size = 33;
  std::array<float, size> inputA = {};
  for (int i = 0; i < size; i++) {
    inputA[i] = static_cast<float>(i);
  }
  std::array<float, size> output = {};

  constexpr int debug_buffer_size = 16;
  // initialize to 0
  std::array<float, debug_buffer_size> debug = {}; // initialize to 0

  // TODO
  // std::array<float, size> debug = {};

  VkBuffer bufferA = mk_buffer(inputA.size(), device,
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  VkBuffer bufferOut = mk_buffer(output.size(), device,
                                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  VkBuffer bufferDebug = mk_buffer(debug.size(), device,
                                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                       VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  auto memory_type = query_memory_type(physical_device);
  if (!memory_type) {
    spdlog::error("Failed to find memory type");
    exit(1);
  }

  VkDeviceMemory memoryA =
      bind_buffer(device, bufferA, memory_type.value(), inputA.size());
  VkDeviceMemory memoryOut =
      bind_buffer(device, bufferOut, memory_type.value(), output.size());
  VkDeviceMemory memoryDebug =
      bind_buffer(device, bufferDebug, memory_type.value(), debug.size());
  copy_to_gpu<size>(device, memoryA, inputA);
  copy_to_gpu<size>(device, memoryOut, output);
  copy_to_gpu<debug_buffer_size>(device, memoryDebug, debug);
  VkShaderModule shader = mk_shader(device, "../build/softmax.spv");

  constexpr int n_bindings = 3;
  VkPipelineLayout pipeline_layout = mk_pipeline_layout<n_bindings>(device);

  VkDescriptorSetLayout descriptor_set_layout =
      mk_descriptor_set_layout<n_bindings>(device);

  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts = {
      descriptor_set_layout};

  // Create descriptor set
  VkDescriptorPool descriptor_pool = mk_descriptor_pool(device);
  VkDescriptorSet descriptor_set =
      mk_descriptor_set(device, descriptor_pool, descriptor_set_layouts);

  VkDescriptorBufferInfo bufferInfoA = mk_descriptor_buffer_info(bufferA);
  VkDescriptorBufferInfo bufferInfoOut = mk_descriptor_buffer_info(bufferOut);
  VkDescriptorBufferInfo bufferInfoDebug =
      mk_descriptor_buffer_info(bufferDebug);
  std::array<VkWriteDescriptorSet, n_bindings> descriptorWrites =
      mk_descriptor_writes<n_bindings>(descriptor_set);
  descriptorWrites[0].pBufferInfo = &bufferInfoA;
  descriptorWrites[1].pBufferInfo = &bufferInfoOut;
  descriptorWrites[2].pBufferInfo = &bufferInfoDebug;
  spdlog::info("descriptorWrites.size(): {}", descriptorWrites.size());
  vkUpdateDescriptorSets(device, n_bindings, descriptorWrites.data(), 0,
                         nullptr);
  spdlog::info("Updated descriptor sets");

  uint32_t wgsize = output.size();
  // uint32_t wgsize = 8;

  const std::array<uint32_t, 3> workgroup_size = {static_cast<uint32_t>(wgsize),
                                                  1, 1};
  VkPipeline pipeline =
      mk_pipeline(device, pipeline_layout, shader, workgroup_size);

  VkCommandPool command_pool = mk_command_pool(device, qfidx);
  VkCommandBuffer command_buffer = mk_command_buffer(device, command_pool);

  // allow command buffer to be executed multiple times
  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
  };
  VkResult result = vkBeginCommandBuffer(command_buffer, &beginInfo);
  check(result, "Begin command buffer.");

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

  // uint32_t group_count_x = (output.size() + 32 - 1) / 32;
  // uint32_t group_count_x = output.size() / workgroup_size[0];
  vkCmdDispatch(command_buffer, size / workgroup_size[0], 1, 1);
  result = vkEndCommandBuffer(command_buffer);
  check(result, "End command buffer.");

  VkQueue queue;
  const uint32_t queue_index = 0;
  vkGetDeviceQueue(device, qfidx, queue_index, &queue);

  std::string input = "";
  while (input != "q") {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    result = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    check(result, "Submit command buffer");

    result = vkQueueWaitIdle(queue);
    check(result, "Wait for queue to become idle");

    copy_to_cpu<size>(device, memoryOut, output);
    copy_to_cpu<debug_buffer_size>(device, memoryDebug, debug);
    spdlog::info("Input: ");
    for (auto &x : inputA) {
      spdlog::info("  {}", x);
    }
    spdlog::info("Debug: ");
    int idx = 0;
    for (auto &x : debug) {
      spdlog::info("  {}", x);
      if (idx % 4 == 3) {
        spdlog::info(" --- ");
      }
      idx++;
    }
    spdlog::info("Output: ");
    for (auto &x : output) {
      spdlog::info("  {}", x);
    }
    // get keyboard input
    std::cout << "Press q to quit, any other key to continue > ";
    std::cin >> input;
  }

  spdlog::info("Done");
}
