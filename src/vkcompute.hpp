#include "spdlog/spdlog.h"
#include <fstream>
#include <iostream>

/* Generic function to check a VkResult and log success/fail condition */
void check(VkResult &result, const char *message) {
  if (result != VK_SUCCESS) {
    spdlog::error("Failed to execute: {}", message);
    spdlog::error("Error code: {}", result);
    exit(1);
  } else {
    spdlog::info("Success: {}", message);
  }
}

template <int n_layers>
VkInstance
mk_vulkan_instance(std::array<char *, n_layers> &validation_layer_names) {
  // application info
  VkApplicationInfo appInfo = {};
  {
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_MAKE_VERSION(1, 3, 236);
  }
  // validate available extensions and layers
  {
    auto extensions_list = vk::enumerateInstanceExtensionProperties();
    auto layers = vk::enumerateInstanceLayerProperties();
    spdlog::info("available extensions:");
    for (const auto &extension : extensions_list) {
      spdlog::info("\t{}", extension.extensionName);
    }
    spdlog::info("available layers:");
    for (const auto &layer : layers) {
      spdlog::info("\t{}", layer.layerName);
    }
    spdlog::info("API version: {}.{}.{}", VK_VERSION_MAJOR(appInfo.apiVersion),
                 VK_VERSION_MINOR(appInfo.apiVersion),
                 VK_VERSION_PATCH(appInfo.apiVersion));
  }
  // Check that the portability extension is available
  {
    auto extensions = vk::enumerateInstanceExtensionProperties();
    bool extensionFound = false;
    for (const auto &extension : extensions) {
      if (strcmp(extension.extensionName,
                 VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
        extensionFound = true;
        break;
      }
    }
    if (!extensionFound) {
      spdlog::error("VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME not "
                    "found!");
      exit(1);
    }
  }
  // Create instance
  VkInstance instance = {};
  {
    const char *validationLayerName = "VK_LAYER_KHRONOS_validation";
    const char *extensionNames[] = {
        VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount =
            static_cast<uint32_t>(validation_layer_names.size()),
        .ppEnabledLayerNames = validation_layer_names.data(),
        .enabledExtensionCount = sizeof(extensionNames) / sizeof(char *),
        .ppEnabledExtensionNames = extensionNames,
    };
    // print enabled extensions
    spdlog::info("Enabled layers :");
    for (uint32_t i = 0; i < createInfo.enabledLayerCount; i++) {
      spdlog::info("\t{}", createInfo.ppEnabledLayerNames[i]);
    }
    spdlog::info("Enabled extensions:");
    for (uint32_t i = 0; i < createInfo.enabledExtensionCount; i++) {
      spdlog::info("\t{}", createInfo.ppEnabledExtensionNames[i]);
    }
    // createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    // create instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    check(result, "vkCreateInstance");
  }
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

template <int size>
void copy_to_gpu(const VkDevice &device, VkDeviceMemory &memory,
                 const std::array<float, size> &input) {
  void *data;
  vkMapMemory(device, memory, 0, sizeof(float) * input.size(), 0, &data);
  memcpy(data, input.data(), sizeof(float) * input.size());
  vkUnmapMemory(device, memory);
  spdlog::info("Memory copied successfully");
}

VkShaderModule mk_shader(VkDevice &device, const std::string &shader_file) {
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
  check(result, "Shader module creation.");
  return shader_module;
}

template <const int n_bindings>
VkPipelineLayout mk_pipeline_layout(VkDevice &device) {
  VkPipelineLayout pipelineLayout = {};
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  // add the descriptor set layout
  VkDescriptorSetLayout descriptor_set_layout = {};
  VkDescriptorSetLayoutBinding uboLayoutBinding[n_bindings] = {};
  // create descriptor set layout for a comput4e shader pipeline
  for (auto idx = 0; idx < n_bindings; ++idx) {
    uboLayoutBinding[idx].binding = idx;
    uboLayoutBinding[idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    uboLayoutBinding[idx].descriptorCount = 1;
    uboLayoutBinding[idx].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    uboLayoutBinding[idx].pImmutableSamplers = nullptr; // Optional
  }
  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = n_bindings;
  layoutInfo.pBindings = uboLayoutBinding;
  if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                  &descriptor_set_layout) != VK_SUCCESS) {
    spdlog::error("Failed to create descriptor set layout");
    exit(1);
  }
  pipelineLayoutInfo.setLayoutCount = 1; // number of descriptor set
  pipelineLayoutInfo.pSetLayouts = &descriptor_set_layout;
  VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr,
                                           &pipelineLayout);
  check(result, "Pipeline layout creation.");
  return pipelineLayout;
}

template <int n_bindings>
VkDescriptorSetLayout mk_descriptor_set_layout(VkDevice &device) {
  VkDescriptorSetLayout descriptorSetLayout = {};
  // bind 3 buffers to the compute shader, 2 inputs, 1 output
  VkDescriptorSetLayoutBinding uboLayoutBinding[n_bindings] = {};
  // create descriptor set layout for a compute shader pipeline
  for (uint32_t idx = 0; idx < n_bindings; ++idx) {
    uboLayoutBinding[idx] = {
        .binding = idx,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr // Optional
    };
  }
  // uboLayoutBinding.pImmutableSamplers = nullptr; // Optional
  VkDescriptorSetLayoutCreateInfo layoutInfo = {};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = n_bindings;
  layoutInfo.pBindings = uboLayoutBinding;
  VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                                &descriptorSetLayout);
  check(result, "Descriptor set layout.");
  return descriptorSetLayout;
}

VkDescriptorBufferInfo mk_descriptor_buffer_info(VkBuffer &buffer) {
  VkDescriptorBufferInfo bufferInfo = {};
  bufferInfo.buffer = buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = VK_WHOLE_SIZE;
  return bufferInfo;
}

template <int n_bindings>
std::array<VkWriteDescriptorSet, n_bindings>
mk_descriptor_writes(VkDescriptorSet &descriptorSet) {
  std::array<VkWriteDescriptorSet, n_bindings> descriptorWrites = {};
  for (auto idx = 0; idx < n_bindings; ++idx) {
    descriptorWrites[idx] = {};
    descriptorWrites[idx].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[idx].dstSet = descriptorSet;
    descriptorWrites[idx].dstBinding = idx;
    descriptorWrites[idx].dstArrayElement = 0;
    descriptorWrites[idx].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[idx].descriptorCount = 1;
    descriptorWrites[idx].pImageInfo = nullptr;       // Optional
    descriptorWrites[idx].pTexelBufferView = nullptr; // Optional
  }
  return descriptorWrites;
}

VkDescriptorPool mk_descriptor_pool(VkDevice &device) {
  VkDescriptorPool descriptorPool = {};
  VkDescriptorPoolSize poolSize = {};
  poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  poolSize.descriptorCount = 3;
  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.poolSizeCount = 1;
  poolInfo.pPoolSizes = &poolSize;
  poolInfo.maxSets = 1;
  VkResult result =
      vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
  check(result, "Descriptor pool creation.");
  return descriptorPool;
}

VkPipeline mk_pipeline(VkDevice &device, VkPipelineLayout &pipelineLayout,
                       VkShaderModule &shaderModule,
                       const std::array<uint32_t, 3> workgroup_size) {
  VkPipeline pipeline = {};
  // Set the workgroup size
  VkSpecializationMapEntry map_entries[3] = {
      {
          .constantID = 0,
          .offset = 0,
          .size = sizeof(uint32_t),
      },
      {
          .constantID = 1,
          .offset = sizeof(uint32_t),
          .size = sizeof(uint32_t),
      },
      {
          .constantID = 2,
          .offset = 2 * sizeof(uint32_t),
          .size = sizeof(uint32_t),
      },
  };
  // print workgroup size
  spdlog::info("Workgroup size: {} {} {}", workgroup_size[0], workgroup_size[1],
               workgroup_size[2]);
  VkSpecializationInfo specialization_info = {
      .mapEntryCount = 3,
      .pMapEntries = map_entries,
      .dataSize = sizeof(uint32_t) * 3,
      .pData = workgroup_size.data(),
  };
  VkPipelineShaderStageCreateInfo shaderStageInfo = {};
  shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStageInfo.module = shaderModule;
  shaderStageInfo.pName = "main";
  shaderStageInfo.pSpecializationInfo = &specialization_info;
  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.stage = shaderStageInfo;
  pipelineInfo.layout = pipelineLayout;
  // pipelineInfo.stage.pSpecializationInfo = &specialization_info;
  // check workgroup size in pipelineInfo and print to log
  const VkSpecializationInfo *spec_info =
      pipelineInfo.stage.pSpecializationInfo;
  uint32_t *data = (uint32_t *)spec_info->pData;
  spdlog::info("Check workgroup size: {} {} {}", data[0], data[1], data[2]);
  VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1,
                                             &pipelineInfo, nullptr, &pipeline);
  if (result != VK_SUCCESS) {
    spdlog::error("Failed to create pipeline: {}", result);
    exit(1);
  }
  spdlog::info("Pipeline created successfully");
  return pipeline;
}

VkDescriptorSet
mk_descriptor_set(VkDevice &device, VkDescriptorPool &pool,
                  std::array<VkDescriptorSetLayout, 1> &layouts) {
  VkDescriptorSet descriptorSet = {};
  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  // array of descriptor set layouts
  // std::vector<VkDescriptorSetLayout> layouts(1, layout);
  allocInfo.pSetLayouts = layouts.data();
  VkResult result =
      vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
  check(result, "Descriptor set allocation.");
  return descriptorSet;
}

VkCommandPool mk_command_pool(VkDevice &device, uint32_t queueFamilyIndex) {
  VkCommandPool commandPool = {};
  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.queueFamilyIndex = queueFamilyIndex;
  poolInfo.flags = 0; // Optional
  VkResult result =
      vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
  check(result, "Create command pool.");
  return commandPool;
}

VkCommandBuffer mk_command_buffer(VkDevice &device,
                                  VkCommandPool &commandPool) {
  VkCommandBuffer commandBuffer = {};
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  VkResult result =
      vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
  check(result, "Command buffer allocation.");
  return commandBuffer;
}

template <int size>
void copy_to_cpu(VkDevice &device, VkDeviceMemory &buffer,
                 std::array<float, size> &data) {
  // copy data from buffer to cpu memory
  void *data_ptr;
  vkMapMemory(device, buffer, 0, sizeof(float) * data.size(), 0, &data_ptr);
  memcpy(data.data(), data_ptr, sizeof(float) * data.size());
  vkUnmapMemory(device, buffer);
  spdlog::info("Data copied to memory");
}
