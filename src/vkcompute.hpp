#include "spdlog/spdlog.h"
#include <exception>
#include <fstream>
#include <iostream>

/* Generic function to check a VkResult and log success/fail condition */
void check(const VkResult &result, const char *message) {
  if (result != VK_SUCCESS) {
    spdlog::error("Failed to execute: {}", message);
    spdlog::error("Error code: {}", result);
    std::runtime_error("Failed to execute: " + std::string(message));
  } else {
    spdlog::info("Success: {}", message);
  }
}

template <size_t n_layers>
VkInstance create_vulkan_instance(
    const std::array<const char *, n_layers> &validation_layer_names,
    uint32_t version) {
  // Application info
  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .apiVersion = version,
  };

  // Validate available extensions and layers
  auto extensions_list = vk::enumerateInstanceExtensionProperties();
  auto layers = vk::enumerateInstanceLayerProperties();

  spdlog::info("Available extensions:");
  for (const auto &extension : extensions_list) {
    spdlog::info("\t{}", extension.extensionName);
  }
  spdlog::info("Available layers:");
  for (const auto &layer : layers) {
    spdlog::info("\t{}", layer.layerName);
  }
  spdlog::info("API version: {}.{}.{}", VK_VERSION_MAJOR(appInfo.apiVersion),
               VK_VERSION_MINOR(appInfo.apiVersion),
               VK_VERSION_PATCH(appInfo.apiVersion));

  // Check that the portability extension is available
  bool portability_extension_found = false;
  auto extensions = vk::enumerateInstanceExtensionProperties();
  for (const auto &extension : extensions) {
    if (strcmp(extension.extensionName,
               VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
      portability_extension_found = true;
      break;
    }
  }
  if (!portability_extension_found) {
    spdlog::warn("VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME not found!");
  }

  // Create instance
  const char *extension_names[] = {
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};

  VkInstanceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = static_cast<uint32_t>(validation_layer_names.size()),
      .ppEnabledLayerNames = validation_layer_names.data(),
      .enabledExtensionCount = std::size(extension_names),
      .ppEnabledExtensionNames = extension_names,
  };

  // Print enabled extensions
  spdlog::info("Enabled layers :");
  for (uint32_t i = 0; i < createInfo.enabledLayerCount; i++) {
    spdlog::info("\t{}", createInfo.ppEnabledLayerNames[i]);
  }
  spdlog::info("Enabled extensions:");
  for (uint32_t i = 0; i < createInfo.enabledExtensionCount; i++) {
    spdlog::info("\t{}", createInfo.ppEnabledExtensionNames[i]);
  }

  VkInstance instance{};
  VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
  check(result, "Create instance");

  return instance;
}

VkPhysicalDevice select_physical_device(VkInstance &instance) {
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(instance, &device_count, nullptr);

  if (device_count == 0) {
    throw std::runtime_error("Failed to find GPUs with Vulkan support.");
  }

  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

  // Log devices found
  for (size_t i = 0; i < devices.size(); ++i) {
    spdlog::info("Device Found Index {}", i);
  }

  // TODO - pick a device based on suitability criterion
  // For now, just pick the first available device
  VkPhysicalDevice selected_device = devices[0];

  // Log selected device properties
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(selected_device, &properties);

  spdlog::info("Physical device count: {}", device_count);
  spdlog::info("Selected device name: {}", properties.deviceName);
  spdlog::info("Max workgroup count x: {}",
               properties.limits.maxComputeWorkGroupCount[0]);
  spdlog::info("Max workgroup count y: {}",
               properties.limits.maxComputeWorkGroupCount[1]);
  spdlog::info("Max workgroup count z: {}",
               properties.limits.maxComputeWorkGroupCount[2]);

  return selected_device;
}

uint32_t find_queue_family(VkPhysicalDevice &physicalDevice,
                           VkQueueFlagBits queueFlags = VK_QUEUE_COMPUTE_BIT) {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count,
                                           nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queue_family_count,
                                           queue_families.data());

  spdlog::info("Queue family count: {}", queue_family_count);

  for (uint32_t i = 0; i < queue_families.size(); ++i) {
    const auto &queue_family = queue_families[i];
    spdlog::info("Queue family {} has {} queues", i, queue_family.queueCount);

    if ((queue_family.queueFlags & queueFlags) && queue_family.queueCount > 0) {
      spdlog::info("Found compute queue family index {}", i);
      return i;
    }
  }

  throw std::runtime_error("Failed to find a suitable queue family.");
}

template <int n_extensions>
VkDevice
create_logical_device(VkPhysicalDevice &physical_device,
                      uint32_t queue_family_index,
                      std::array<const char *, n_extensions> extension_names) {
  float queue_priority = 1.0f;

  VkDeviceQueueCreateInfo queue_create_info{};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = queue_family_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;

  spdlog::info("# of extensions: {}", extension_names.size());

  VkDeviceCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  create_info.queueCreateInfoCount = 1;
  create_info.pQueueCreateInfos = &queue_create_info;
  create_info.enabledLayerCount = 0;
  create_info.ppEnabledLayerNames = nullptr;
  create_info.enabledExtensionCount =
      static_cast<uint32_t>(extension_names.size());
  create_info.ppEnabledExtensionNames = extension_names.data();
  create_info.pEnabledFeatures = nullptr;

  VkDevice device;
  VkResult result =
      vkCreateDevice(physical_device, &create_info, nullptr, &device);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create logical device.");
  }

  return device;
}

VkBuffer create_buffer(int size, VkDevice &device, VkBufferUsageFlags usage) {
  VkBufferCreateInfo buffer_create_info{};
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.size = sizeof(float) * size;
  buffer_create_info.usage = usage;
  buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  VkBuffer buffer;
  VkResult result =
      vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer);
  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create buffer.");
  }
  return buffer;
}

std::optional<uint32_t> query_memory_type(VkPhysicalDevice &physicalDevice) {
  // Find a memory type that satisfies the requirements
  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memory_properties);

  // Iterate through the memory types and find one that is both host visible and
  // host coherent
  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    bool host_visible = memory_properties.memoryTypes[i].propertyFlags &
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    bool host_coherent = memory_properties.memoryTypes[i].propertyFlags &
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    // Log memory type information
    spdlog::info("Memory type {}: host_visible={}, host_coherent={}", i,
                 host_visible, host_coherent);

    if (host_visible && host_coherent) {
      spdlog::info("Selected memory index: {}", i);
      return i;
    }
  }

  // No suitable memory type found
  spdlog::warn("No suitable memory type found.");
  return std::nullopt;
}

VkDeviceMemory bind_buffer(VkDevice &device, VkBuffer &buffer, int memory_type,
                           int size) {
  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

  VkMemoryAllocateInfo memory_allocate_info{};
  memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_allocate_info.allocationSize = memory_requirements.size;
  memory_allocate_info.memoryTypeIndex = memory_type;

  spdlog::info("Memory requirements size: {}", memory_requirements.size);

  VkDeviceMemory memory;
  VkResult result =
      vkAllocateMemory(device, &memory_allocate_info, nullptr, &memory);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate GPU memory.");
  }

  result = vkBindBufferMemory(device, buffer, memory, 0);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to bind memory to buffers.");
  }

  spdlog::info("Memory bound to buffers successfully");
  return memory;
}

template <size_t size>
void copy_to_gpu(const VkDevice &device, VkDeviceMemory &memory,
                 const std::array<float, size> &input) {
  void *data;
  VkResult result =
      vkMapMemory(device, memory, 0, sizeof(float) * input.size(), 0, &data);
  check(result, "Map data to GPU memory");
  memcpy(data, input.data(), sizeof(float) * input.size());
  vkUnmapMemory(device, memory);
  spdlog::info("Memory copied successfully");
}

VkShaderModule create_shader_module(VkDevice &device,
                                    const std::string &shader_file) {
  // Read shader file
  std::ifstream file(shader_file, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("Failed to open shader file: " + shader_file);
  }

  size_t file_size = static_cast<size_t>(file.tellg());
  std::vector<char> shader_data(file_size);

  file.seekg(0);
  file.read(shader_data.data(), file_size);
  file.close();

  // Create shader module
  VkShaderModuleCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create_info.codeSize = shader_data.size();
  create_info.pCode = reinterpret_cast<const uint32_t *>(shader_data.data());

  VkShaderModule shader_module;
  VkResult result =
      vkCreateShaderModule(device, &create_info, nullptr, &shader_module);

  check(result, "Create shader module");
  return shader_module;
}

template <size_t n_bindings>
VkPipelineLayout create_pipeline_layout(VkDevice &device) {
  VkPipelineLayout pipelineLayout{};
  VkPipelineLayoutCreateInfo pipelineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  // add the descriptor set layout
  VkDescriptorSetLayout descriptor_set_layout{};
  std::array<VkDescriptorSetLayoutBinding, n_bindings> uboLayoutBindings{};

  // create descriptor set layout for a compute shader pipeline
  for (size_t idx = 0; idx < n_bindings; ++idx) {
    uboLayoutBindings[idx] = {
        .binding = static_cast<uint32_t>(idx),
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr // Optional
    };
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(n_bindings),
      .pBindings = uboLayoutBindings.data()};

  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                  &descriptor_set_layout) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor set layout.");
  }

  pipelineLayoutInfo.setLayoutCount = 1; // number of descriptor set
  pipelineLayoutInfo.pSetLayouts = &descriptor_set_layout;
  VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                           &pipelineLayout);

  if (result != VK_SUCCESS) {
    throw std::runtime_error("Failed to create pipeline layout.");
  }

  return pipelineLayout;
}

template <size_t n_bindings>
VkDescriptorSetLayout create_descriptor_set_layout(VkDevice &device) {
  std::array<VkDescriptorSetLayoutBinding, n_bindings> uboLayoutBindings{};

  for (uint32_t idx = 0; idx < n_bindings; ++idx) {
    uboLayoutBindings[idx] = {.binding = idx,
                              .descriptorType =
                                  VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                              .descriptorCount = 1,
                              .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
                              .pImmutableSamplers = nullptr};
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(n_bindings),
      .pBindings = uboLayoutBindings.data()};

  VkDescriptorSetLayout descriptorSetLayout{};
  VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                                &descriptorSetLayout);
  check(result, "Descriptor set layout creation.");

  return descriptorSetLayout;
}

VkDescriptorBufferInfo create_descriptor_buffer_info(VkBuffer &buffer) {
  VkDescriptorBufferInfo bufferInfo{
      .buffer = buffer, .offset = 0, .range = VK_WHOLE_SIZE};

  return bufferInfo;
}

template <size_t n_bindings>
std::array<VkWriteDescriptorSet, n_bindings>
create_descriptor_writes(VkDescriptorSet &descriptorSet) {
  std::array<VkWriteDescriptorSet, n_bindings> descriptorWrites{};

  for (size_t idx = 0; idx < n_bindings; ++idx) {
    descriptorWrites[idx] = {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                             .dstSet = descriptorSet,
                             .dstBinding = static_cast<uint32_t>(idx),
                             .dstArrayElement = 0,
                             .descriptorType =
                                 VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             .descriptorCount = 1,
                             .pImageInfo = nullptr,
                             .pTexelBufferView = nullptr};
  }

  return descriptorWrites;
}

VkDescriptorPool create_descriptor_pool(VkDevice &device) {
  VkDescriptorPoolSize poolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .descriptorCount = 3};

  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = 1,
      .pPoolSizes = &poolSize,
      .maxSets = 1};

  VkDescriptorPool descriptorPool{};
  VkResult result =
      vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
  check(result, "Descriptor pool creation.");

  return descriptorPool;
}

VkPipeline create_pipeline(VkDevice &device, VkPipelineLayout &pipelineLayout,
                           VkShaderModule &shaderModule,
                           const std::array<uint32_t, 3> &workgroup_size) {
  std::array<VkSpecializationMapEntry, 3> map_entries{{
      {.constantID = 0, .offset = 0, .size = sizeof(uint32_t)},
      {.constantID = 1, .offset = sizeof(uint32_t), .size = sizeof(uint32_t)},
      {.constantID = 2,
       .offset = 2 * sizeof(uint32_t),
       .size = sizeof(uint32_t)},
  }};

  spdlog::info("Workgroup size: {} {} {}", workgroup_size[0], workgroup_size[1],
               workgroup_size[2]);

  VkSpecializationInfo specialization_info{
      .mapEntryCount = static_cast<uint32_t>(map_entries.size()),
      .pMapEntries = map_entries.data(),
      .dataSize = sizeof(uint32_t) * workgroup_size.size(),
      .pData = workgroup_size.data(),
  };

  VkPipelineShaderStageCreateInfo shaderStageInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
      .module = shaderModule,
      .pName = "main",
      .pSpecializationInfo = &specialization_info,
  };

  VkComputePipelineCreateInfo pipelineInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .stage = shaderStageInfo,
      .layout = pipelineLayout,
  };

  const VkSpecializationInfo *spec_info =
      pipelineInfo.stage.pSpecializationInfo;
  const uint32_t *data = reinterpret_cast<const uint32_t *>(spec_info->pData);
  spdlog::info("Check workgroup size: {} {} {}", data[0], data[1], data[2]);

  VkPipeline pipeline{};
  VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr, &pipeline);
  check(result, "Pipeline creation.");

  spdlog::info("Pipeline created successfully");
  return pipeline;
}

VkDescriptorSet
create_descriptor_set(VkDevice &device, VkDescriptorPool &pool,
                      const std::array<VkDescriptorSetLayout, 1> &layouts) {
  VkDescriptorSetAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
      .pSetLayouts = layouts.data(),
  };

  VkDescriptorSet descriptorSet{};
  VkResult result =
      vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
  check(result, "Descriptor set allocation.");

  return descriptorSet;
}

VkCommandPool create_command_pool(VkDevice &device, uint32_t queueFamilyIndex) {
  VkCommandPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = queueFamilyIndex,
      .flags = 0, // Optional
  };

  VkCommandPool commandPool{};
  VkResult result =
      vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
  check(result, "Create command pool.");

  return commandPool;
}

VkCommandBuffer create_command_buffer(VkDevice &device,
                                      VkCommandPool &commandPool) {
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer commandBuffer{};
  VkResult result =
      vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
  check(result, "Command buffer allocation.");

  return commandBuffer;
}

template <size_t size>
void copy_to_cpu(VkDevice &device, VkDeviceMemory &buffer,
                 std::array<float, size> &data) {
  void *data_ptr;
  VkDeviceSize dataSize = sizeof(float) * data.size();
  vkMapMemory(device, buffer, 0, dataSize, 0, &data_ptr);
  memcpy(data.data(), data_ptr, dataSize);
  vkUnmapMemory(device, buffer);
  spdlog::info("Data copied to memory");
}
