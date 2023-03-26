#version 450

layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(std430, binding = 0) buffer InA {
  float data[];
} buffer_a;

layout(std430, binding = 1) buffer InB {
  float data[];
} buffer_b;

layout(std430, binding = 2) buffer Out {
  float data[];
} buffer_out;

shared float products[2048]; // TODO - make this size dynamic and based on the input buffer size

void main() {

  const uint index = gl_GlobalInvocationID.x;
  const uint workgroup_idx = gl_WorkGroupID.x;
  const uint batch_size = gl_WorkGroupSize.x; // 1 batch = 1 workgroup
  const uint num_work_items = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
  const uint local_idx = gl_LocalInvocationID.x;

  products[index] = buffer_a.data[index] * buffer_b.data[index];

  barrier();

  // zip product
  // sum for each workgroup (batch)
  if (local_idx == 0) {
    float sum = 0.0;
    for (uint i = 0; i < batch_size; i++) {
      sum += products[index + i];
    }
    buffer_out.data[workgroup_idx] = sum;
    // buffer_out.data[workgroup_idx] = -1.0;
  }

}
