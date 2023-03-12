#version 450

// TODO - enable in vulkan
#extension GL_EXT_debug_printf : enable

layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// array input
layout(std430, binding = 0) buffer Data {
	float data[];
} data_in;

layout(std430, binding = 1) buffer Out {
	float data[];
} data_out;

layout(std430, binding = 2) buffer Debug {
	float data[];
} debug;

shared float shared_exp_data[512];
shared float partial_sums[512];

void main () {
  const uint global_idx = gl_GlobalInvocationID.x;
  const uint local_idx = gl_LocalInvocationID.x;
  const uint workgroup_idx = gl_WorkGroupID.x;
  const uint n_workgroups = gl_NumWorkGroups.x;
  const uint workgroup_size = gl_WorkGroupSize.x;
    
  shared_exp_data[global_idx] = exp(data_in.data[global_idx]);
  partial_sums[local_idx] = shared_exp_data[global_idx]
  float value = shared_exp_data[global_idx];

  for (uint stride = 1; stride <= gl_WorkGroupSize.x; stride *= 2) {
      barrier();
      uint index = (local_idx + 1) * stride * 2 - 1;
      if (index < gl_WorkGroupSize.x) {
          partial_sums[index] += partial_sums[index - stride];
      }
  }

  barrier();

  for (uint stride = gl_WorkGroupSize.x / 2; stride > 0; stride /= 2) {
      barrier(); // Ensure all threads have computed partial sums
      if (local_idx < stride) {
          partial_sums[local_idx] += partial_sums[local_idx + stride];
      }
  }
  const float total = partial_sums[0];

  data_out.data[global_idx] = shared_exp_data[global_idx] / total;
}
