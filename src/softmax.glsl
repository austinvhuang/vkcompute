#version 450

// TODO - enable in vulkan
#extension GL_EXT_debug_printf : enable

layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

// array input
layout(std430, binding = 0) buffer Data {
	float data[];
} data;

layout(std430, binding = 1) buffer Out {
	float data[];
} data_out;

layout(std430, binding = 2) buffer Debug {
	float data[];
} debug;

shared float shared_exp_data[512];
shared float shared_sum_data[512];

void main () {
	const uint global_index = gl_GlobalInvocationID.x;
	const uint local_index = gl_LocalInvocationID.x;
	// const uint group_size = gl_WorkGroupSize.x;
    
	shared_exp_data[global_index] = exp(data.data[global_index]);
  shared_sum_data[global_index] = shared_exp_data[global_index];
  
  // Parallel reduction - TODO - fix for cases where workgroup size < total size
	for (uint stride = 1; stride < gl_WorkGroupSize.x; stride *= 2) {
		uint index = 2 * stride * local_index;
		if (index + stride < gl_WorkGroupSize.x) {
			shared_sum_data[index] += shared_sum_data[index + stride];
		}
		memoryBarrierShared();
	}

  data_out.data[global_index] = shared_exp_data[global_index] / shared_sum_data[0];

}
