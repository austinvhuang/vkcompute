
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
  spdlog::set_pattern("[%^%l%$] [%H:%M:%S] %v");
  auto file_logger =
      spdlog::basic_logger_mt("basic_logger", "logs/vulkan_log.txt");
  auto console = spdlog::stdout_color_mt("console");
  spdlog::set_default_logger(console);
  spdlog::flush_every(std::chrono::seconds(3));
}

int main() {
  setup_logging();

  /*
   * Setup vulkan instance, physical and logical devices.
   */

  constexpr int n_layers = 1;
  std::array<char *, n_layers> instance_layer_names = {
      const_cast<char *>("VK_LAYER_KHRONOS_validation")};
  VkInstance instance = create_vulkan_instance<n_layers>(instance_layer_names);
  VkPhysicalDevice physical_device = select_physical_device(instance);
  uint32_t qfidx = find_queue_family(physical_device);
  constexpr int n_device_extensions = 1;
  // add portability and printf
  std::array<const char *, n_device_extensions> extension_names = {
      const_cast<char *>(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)};
  VkDevice device = create_logical_device<n_device_extensions>(
      physical_device, qfidx, extension_names);

  /*
   * Create host-side array resources (C++ arrays), vkBuffer handles to them,
   * and device memory handles for associated GPU memory.
   */

  constexpr int size = 16;
  std::array<float, size> input_a = {};
  for (int i = 0; i < size; i++) {
    input_a[i] = static_cast<float>(i);
  }
  std::array<float, size> output = {};

  VkBuffer buffer_in = create_buffer(input_a.size(), device,
                                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  VkBuffer buffer_out = create_buffer(output.size(), device,
                                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

  auto memory_type = query_memory_type(physical_device);
  if (!memory_type) {
    spdlog::error("Failed to find memory type");
    exit(1);
  }
  VkDeviceMemory memory_in =
      bind_buffer(device, buffer_in, memory_type.value(), input_a.size());
  VkDeviceMemory memory_out =
      bind_buffer(device, buffer_out, memory_type.value(), output.size());
  copy_to_gpu<size>(device, memory_in, input_a);
  copy_to_gpu<size>(device, memory_out, output);

  /*
   * Create shader module and pipeline for computation.
   */

  VkShaderModule shader = create_shader_module(device, "build/softmax.spv");
  constexpr int n_bindings = 2;
  VkPipelineLayout pipeline_layout = create_pipeline_layout<n_bindings>(device);
  VkDescriptorSetLayout descriptor_set_layout =
      create_descriptor_set_layout<n_bindings>(device);
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts = {
      descriptor_set_layout};
  VkDescriptorPool descriptor_pool = create_descriptor_pool(device);
  VkDescriptorSet descriptor_set =
      create_descriptor_set(device, descriptor_pool, descriptor_set_layouts);
  VkDescriptorBufferInfo bufferinfo_in =
      create_descriptor_buffer_info(buffer_in);
  VkDescriptorBufferInfo bufferinfo_out =
      create_descriptor_buffer_info(buffer_out);
  std::array<VkWriteDescriptorSet, n_bindings> descriptorWrites =
      create_descriptor_writes<n_bindings>(descriptor_set);
  descriptorWrites[0].pBufferInfo = &bufferinfo_in;
  descriptorWrites[1].pBufferInfo = &bufferinfo_out;
  spdlog::info("descriptorWrites.size(): {}", descriptorWrites.size());
  vkUpdateDescriptorSets(device, n_bindings, descriptorWrites.data(), 0,
                         nullptr);
  uint32_t wgsize = output.size();
  const std::array<uint32_t, 3> workgroup_size = {static_cast<uint32_t>(wgsize),
                                                  1, 1};
  VkPipeline pipeline =
      create_pipeline(device, pipeline_layout, shader, workgroup_size);

  /*
   * Create and record a command buffer corresponding to the compute shader
   * computation and a device queue ot submit the computation.
   */

  VkCommandPool command_pool = create_command_pool(device, qfidx);
  VkCommandBuffer command_buffer = create_command_buffer(device, command_pool);
  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      // allow command buffer to be executed multiple times
      // -
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
  };
  VkResult result = vkBeginCommandBuffer(command_buffer, &beginInfo);
  check(result, "Begin command buffer.");

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

  vkCmdDispatch(command_buffer, size / workgroup_size[0], 1, 1);
  result = vkEndCommandBuffer(command_buffer);
  check(result, "End command buffer.");

  VkQueue queue;
  const uint32_t queue_index = 0;
  vkGetDeviceQueue(device, qfidx, queue_index, &queue);

  /*
   * Main execution loop - submit the computation to the queue, copy the results
   * to the host, display them. Ask user to re-run the computation or exit the
   * program.
   */

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

    copy_to_cpu<size>(device, memory_in, input_a);
    copy_to_cpu<size>(device, memory_out, output);
    spdlog::info("Input: ");
    int idx = 0;
    for (auto &x : input_a) {
      spdlog::info("{} : {}", idx, x);
      idx++;
    }
    spdlog::info("Output: ");
    idx = 0;
    for (auto &x : output) {
      spdlog::info("{} : {}", idx, x);
      idx++;
    }
    // get keyboard input
    std::cout << "Enter q to quit, anything else to re-run computation > ";
    std::getline(std::cin, input);
  }
  spdlog::info("Done");
}
