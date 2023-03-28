
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

void main() {
  const uint index = gl_GlobalInvocationID.x;
  buffer_out.data[index] = buffer_a.data[index] * buffer_b.data[index];
}
