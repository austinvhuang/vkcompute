#version 450

// #extension GL_EXT_debug_printf : enable
// #extension GL_EXT_shader_atomic_float : enable

layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(std430, binding = 0) buffer Data {
	float data[];
} data_in;

layout(std430, binding = 1) buffer Out {
	float data[];
} data_out;

shared float exp_data[512];
shared float partial_sums[512];

void main () {
  const uint global_idx = gl_GlobalInvocationID.x;
  const uint local_idx = gl_LocalInvocationID.x;
  const uint workgroup_idx = gl_WorkGroupID.x;
  const uint n_workgroups = gl_NumWorkGroups.x;
  const uint workgroup_size = gl_WorkGroupSize.x;
    
  exp_data[global_idx] = exp(data_in.data[global_idx]);
  partial_sums[global_idx] = exp_data[global_idx];

  for (uint stride = 1; stride < workgroup_size; stride *= 2) {
      barrier();
      if (local_idx % (2 * stride) == 0 && local_idx + stride < workgroup_size) {
        partial_sums[global_idx] = partial_sums[global_idx] + partial_sums[global_idx + stride];
      }
  }

  data_out.data[global_idx] = exp_data[global_idx] / partial_sums[0];
}
