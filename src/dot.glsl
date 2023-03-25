#version 450

layout (local_size_x_id = 0, local_size_y_id = 1, local_size_z_id = 2) in;

layout(std430, binding = 1) buffer InA {
  float data[];
} buffer_a;

layout(std430, binding = 2) buffer InB {
  float data[];
} buffer_b;

layout(std430, binding = 3) buffer Out {
  float data[];
} buffer_out;

void main() {

  const uint index = gl_GlobalInvocationID.x;
  const uint num_work_items = gl_NumWorkGroups.x * gl_WorkGroupSize.x;

  // compute dot product
  buffer_out.data[index] = buffer_a.data[index] * buffer_b.data[index];

  // wait for all threads to finish
  barrier();

  if (index == 0) {
    // compute sum
    float sum = 0.0;
    for (uint i = 0; i < num_work_items; i++) {
      sum += buffer_out.data[i];
    }
    buffer_out.data[0] = sum;
  }

}
