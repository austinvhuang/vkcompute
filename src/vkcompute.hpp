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
    spdlog::info("Available extensions:");
    for (const auto &extension : extensions) {
      spdlog::info("\t{}", extension.extensionName);
      if (strcmp(extension.extensionName,
                 VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
        extensionFound = true;
        // break;
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

VkPhysicalDevice mk_physcal_device(VkInstance &instance) {
  vk::PhysicalDevice physicalDevice = VK_NULL_HANDLE;
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    throw std::runtime_error("failed to find GPUs with Vulkan support!");
  }
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
  int idx = 0;
  for (const auto &device : devices) {
    spdlog::info("Device Found Index {}", idx);
    ++idx;
  }
  assert(devices.size() > 0); // should be satisfied if deviceCount > 0
  // TODO - pick a device based on suitability criterion
  physicalDevice = devices[0];
  spdlog::info("Physical device count: {}", deviceCount);
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(physicalDevice, &properties);
  spdlog::info("Device name: {}", properties.deviceName);
  spdlog::info("Max workgroup count x: {}",
               properties.limits.maxComputeWorkGroupCount[0]);
  spdlog::info("Max workgroup count y: {}",
               properties.limits.maxComputeWorkGroupCount[1]);
  spdlog::info("Max workgroup count z: {}",
               properties.limits.maxComputeWorkGroupCount[2]);
  return physicalDevice;
}

uint32_t find_queue_family(VkPhysicalDevice &physicalDevice,
                           VkQueueFlagBits queueFlags = VK_QUEUE_COMPUTE_BIT) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                           queueFamilies.data());
  int i = 0;
  int queue_family_index = -1;
  spdlog::info("queue family count: {}", queueFamilyCount);
  for (const auto &queueFamily : queueFamilies) {
    spdlog::info("queue family {} has {} queues", i,
                 queueFamilies[i].queueCount);
    if (queueFamily.queueFlags & queueFlags && queueFamily.queueCount > 0 &&
        queue_family_index == -1) {
      spdlog::info("found compute queue family index {}", i);
      queue_family_index = i;
    }
    i++;
  }
  return queue_family_index;
}

template <int n_extensions>
VkDevice mk_logical_device(VkPhysicalDevice &physicalDevice,
                           uint32_t queue_family_index,
                           std::array<char *, n_extensions> extension_names) {
  VkDevice device = {};
  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = queue_family_index,
      .queueCount = 1,
      .pQueuePriorities = &queuePriority,
  };
  /*
  const char *extensionNames[] = {
      VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME,
  };
  */
  spdlog::info("# of extensions: {}", extension_names.size());
  VkDeviceCreateInfo createInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueCreateInfo,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = static_cast<uint32_t>(extension_names.size()),
      .ppEnabledExtensionNames = extension_names.data(),
      .pEnabledFeatures = nullptr,
      /*
.flags = VK_DEVICE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR, // TODO - this
                                                       // needed?
      */
  };
  VkResult result =
      vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
  check(result, "Logical device creation.");
  return device;
}

VkBuffer mk_buffer(int size, VkDevice &device, VkBufferUsageFlags usage) {
  VkBuffer buffer = {};
  VkBufferCreateInfo bufferCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = sizeof(float) * size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, &buffer);
  check(result, "Buffer creation.");
  return buffer;
}

std::optional<int> query_memory_type(VkPhysicalDevice &physicalDevice) {
  // find a memory type that satisfies the requirements
  VkPhysicalDeviceMemoryProperties memoryProperties = {};
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
  int index = -1;
  for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
    // print name of memory type
    spdlog::info("Memory type {}:", i);
    if (memoryProperties.memoryTypes[i].propertyFlags &
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
      spdlog::info("Memory type set (host visible): {}", i);
    } else {
      spdlog::info("Memory type skipped (not host-visible): {}", i);
    }
    if ((memoryProperties.memoryTypes[i].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      spdlog::info("Memory type set (coherent): {}", i);
    } else {
      spdlog::info("Memory type skipped (not coherent): {}", i);
    }
    if ((memoryProperties.memoryTypes[i].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
        (memoryProperties.memoryTypes[i].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
      index = i;
      spdlog::info("Setting memory index to: {}", i);
    }
  }
  if (index == -1) {
    return std::nullopt;
  }
  return std::optional<int>(index);
}

VkDeviceMemory bind_buffer(VkDevice &device, VkBuffer &buffer, int memory_type,
                           int size) {
  VkDeviceMemory memory = {};
  VkMemoryRequirements memoryRequirements = {};
  // TODO - setup size correctly as a parameter
  memoryRequirements.size = size * sizeof(float);
  VkMemoryAllocateInfo memoryAllocateInfo = {};
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex = memory_type;
  vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
  memoryAllocateInfo.allocationSize = memoryRequirements.size;
  spdlog::info("Memory requirements size: {}", memoryRequirements.size);
  VkResult result =
      vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &memory);
  check(result, "Allocate GPU memory");
  // bind memory to buffers
  vkBindBufferMemory(device, buffer, memory, 0);
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
  std::ifstream file(shader_file, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    std::cerr << "Failed to open file" << std::endl;
    exit(1);
  }
  size_t fileSize = (size_t)file.tellg();
  std::string shader(fileSize, ' ');
  file.seekg(0);
  file.read(shader.data(), fileSize);
  file.close();
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = shader.size();
  createInfo.pCode = reinterpret_cast<const uint32_t *>(shader.data());
  VkShaderModule shaderModule;
  VkResult result =
      vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
  check(result, "Shader module creation.");
  return shaderModule;
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
  if (result != VK_SUCCESS) {
    spdlog::error("Failed to create descriptor pool: {}", result);
    exit(1);
  }
  spdlog::info("Descriptor pool created successfully");
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
