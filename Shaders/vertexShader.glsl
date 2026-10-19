#version 330 core
///////////////////////////////////////////////////////////////////////////////
// vertexShader.glsl
// ============
// place each vertex in the window and pass its position, normal, and
// texture coordinate on to the fragment shader
//
// This is the 7-1 project's own copy of the course vertex shader in
// Utilities/shaders, which other projects still use unchanged. It adds two
// things: normals are turned with the shape by normalMatrix, and each
// vertex's position on the unscaled mesh is passed on, so the fragment
// shader can rebuild exact normals for the cylinder, cone, and tapered
// cylinder meshes, make texture coordinates for box faces, cylinder
// caps, tapered sides, and the sphere, and fade the lamp's glow (see
// fragmentShader.glsl)
///////////////////////////////////////////////////////////////////////////////

layout (location = 0) in vec3 inVertexPosition;
layout (location = 1) in vec3 inVertexNormal;
layout (location = 2) in vec2 inTextureCoordinate;

out vec3 fragmentPosition;
out vec3 fragmentVertexNormal;
out vec3 fragmentMeshPosition;
out vec2 fragmentTextureCoordinate;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
// the inverse transpose of the model matrix's rotation and scale, set by
// SceneManager::SetModelMatrix(), which every transformation helper calls.
// It turns a normal with its shape and keeps it at right angles to the
// surface when the shape is stretched more along one axis than another,
// which the model matrix itself would not
uniform mat3 normalMatrix;

void main()
{
   fragmentPosition = vec3(model * vec4(inVertexPosition, 1.0));
   gl_Position = projection * view * model * vec4(inVertexPosition, 1.0f);
   fragmentVertexNormal = normalMatrix * inVertexNormal;
   fragmentMeshPosition = inVertexPosition;
   fragmentTextureCoordinate = inTextureCoordinate;
}
