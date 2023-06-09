
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

  VkInstance instance =
      vkc::create_vulkan_instance(VK_MAKE_API_VERSION(1, 3, 236, 0));
  VkPhysicalDevice physical_device = vkc::select_physical_device(instance);
  uint32_t qfidx = vkc::find_queue_family(physical_device);
  VkDevice device = vkc::create_logical_device(physical_device, qfidx);

  /*
   * Create host-side array resources (C++ arrays), vkBuffer handles to them,
   * and device memory handles for associated GPU memory.
   */

  constexpr size_t size = 8;
  std::array<float, size> input_a{};
  for (size_t i = 0; i < size; i++) {
    // for test input values, fill the array with increasing values from 0 to
    // size
    input_a[i] = static_cast<float>(i);
  }
  std::array<float, size> output{};
  VkBuffer buffer_in = vkc::create_buffer(input_a.size(), device,
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  VkBuffer buffer_out = vkc::create_buffer(
      output.size(), device,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  auto memory_type = vkc::query_memory_type(physical_device);
  if (!memory_type) {
    spdlog::error("Failed to find memory type");
    std::runtime_error("Failed to find memory type");
  }
  VkDeviceMemory memory_in =
      vkc::bind_buffer(device, buffer_in, memory_type.value(), input_a.size());
  VkDeviceMemory memory_out =
      vkc::bind_buffer(device, buffer_out, memory_type.value(), output.size());
  vkc::copy_to_gpu<size>(device, memory_in, input_a);

  /*
   * Create descriptor set layout, descriptor pool, and descriptor set for
   * binding the input and output buffers to the compute shader.
   */

  constexpr size_t n_bindings = 2; // 2 bindings: input and output
  VkPipelineLayout pipeline_layout =
      vkc::create_pipeline_layout<n_bindings>(device);
  VkDescriptorSetLayout descriptor_set_layout =
      vkc::create_descriptor_set_layout<n_bindings>(device);
  std::array<VkDescriptorSetLayout, 1> descriptor_set_layouts = {
      descriptor_set_layout};
  VkDescriptorPool descriptor_pool = vkc::create_descriptor_pool(device);
  VkDescriptorSet descriptor_set = vkc::create_descriptor_set(
      device, descriptor_pool, descriptor_set_layouts);
  VkDescriptorBufferInfo bufferinfo_in =
      vkc::create_descriptor_buffer_info(buffer_in);
  VkDescriptorBufferInfo bufferinfo_out =
      vkc::create_descriptor_buffer_info(buffer_out);
  std::array<VkWriteDescriptorSet, n_bindings> descriptorWrites =
      vkc::create_descriptor_writes<n_bindings>(descriptor_set);
  descriptorWrites[0].pBufferInfo = &bufferinfo_in;
  descriptorWrites[1].pBufferInfo = &bufferinfo_out;
  vkUpdateDescriptorSets(device, n_bindings, descriptorWrites.data(), 0,
                         nullptr);
  spdlog::info("Created descriptor set.");

  /*
   * Create shader module and pipeline for computation.
   */

  uint32_t wgsize = static_cast<uint32_t>(output.size());
  std::array<uint32_t, 3> workgroup_size = {wgsize, 1, 1};
  VkShaderModule shader =
      vkc::create_shader_module(device, "build/softmax.spv");
  VkPipeline pipeline =
      vkc::create_pipeline(device, pipeline_layout, shader, workgroup_size);

  /*
   * Create a command buffer and corresponding command pool for submitting
   * commands to the GPU.
   */

  VkCommandPool command_pool = vkc::create_command_pool(device, qfidx);
  VkCommandBuffer command_buffer =
      vkc::create_command_buffer(device, command_pool);

  /*
   * Record commands to the command buffer.
   */

  VkCommandBufferBeginInfo beginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags =
          VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT, // Allow command buffer
                                                        // to be executed
                                                        // multiple times
  };
  VkResult result = vkBeginCommandBuffer(command_buffer, &beginInfo);
  vkc::check(result, "Begin command buffer.");

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
  vkCmdDispatch(command_buffer, size / workgroup_size[0], 1, 1);

  result = vkEndCommandBuffer(command_buffer);
  vkc::check(result, "End command buffer.");

  /*
   * Create a queue for submitting command buffers to the GPU.
   * The queue is created from the device and the queue family index.
   */

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
    vkc::copy_to_cpu<size>(device, memory_out, output);
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

    // Test re-using the computation or let the user quit
    std::cout << "Enter q to quit, anything else to re-run computation > ";
    std::getline(std::cin, input);
  }

  spdlog::info("Done");
}
