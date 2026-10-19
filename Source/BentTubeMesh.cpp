///////////////////////////////////////////////////////////////////////////////
// bentTubeMesh.cpp
// ============
// build and draw a bent tube: an arc of a torus, swept through any angle
///////////////////////////////////////////////////////////////////////////////

#include "BentTubeMesh.h"

#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

/***********************************************************
 *  BentTubeMesh()
 *
 *  The constructor for the class. No GPU memory is used
 *  until Load() builds the arc.
 ***********************************************************/
BentTubeMesh::BentTubeMesh()
{
	m_vao = 0;
	m_vertexBuffer = 0;
	m_indexBuffer = 0;
	m_indexCount = 0;
}

/***********************************************************
 *  ~BentTubeMesh()
 *
 *  The destructor for the class. It frees the GPU buffers,
 *  so the object must be destroyed while the OpenGL context
 *  that built them still exists.
 ***********************************************************/
BentTubeMesh::~BentTubeMesh()
{
	Release();
}

/***********************************************************
 *  Load()
 *
 *  This method is used for building the arc's vertices and
 *  triangles and loading them into GPU memory. A ring of
 *  vertices goes around the tube at each step along the arc,
 *  and each ring repeats its first vertex at v = 1, so the
 *  texture meets itself without a seam where v wraps. Each
 *  vertex stores the tube's exact normal: the direction from
 *  the ring's center line out to the vertex.
 ***********************************************************/
void BentTubeMesh::Load(
	float sweepDegrees,
	float tubeRadius,
	int arcSegments,
	int tubeSegments)
{
	Release();

	// position (3), normal (3), and texture coordinate (2), the same
	// layout the course meshes use, so the same shaders draw both
	std::vector<GLfloat> vertices;
	std::vector<GLuint> indices;
	float sweep = glm::radians(sweepDegrees);

	for (int i = 0; i <= arcSegments; i++)
	{
		float u = static_cast<float>(i) / arcSegments;
		float ringAngle = u * sweep;
		// the center line of the tube, and the outward direction there
		glm::vec3 outward(glm::cos(ringAngle), glm::sin(ringAngle), 0.0f);

		for (int j = 0; j <= tubeSegments; j++)
		{
			float v = static_cast<float>(j) / tubeSegments;
			float tubeAngle = v * glm::two_pi<float>();
			glm::vec3 normal = outward * glm::cos(tubeAngle) + glm::vec3(0.0f, 0.0f, glm::sin(tubeAngle));
			glm::vec3 position = outward + normal * tubeRadius;

			vertices.insert(vertices.end(), { position.x, position.y, position.z,
				normal.x, normal.y, normal.z, u, v });
		}
	}

	// two triangles for each patch between neighboring rings, wound
	// counterclockwise when seen from outside the tube
	int ringSize = tubeSegments + 1;
	for (int i = 0; i < arcSegments; i++)
	{
		for (int j = 0; j < tubeSegments; j++)
		{
			GLuint a = i * ringSize + j;
			GLuint b = (i + 1) * ringSize + j;
			indices.insert(indices.end(), { a, b, b + 1, a, b + 1, a + 1 });
		}
	}
	m_indexCount = static_cast<GLsizei>(indices.size());

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	glGenBuffers(1, &m_vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

	glGenBuffers(1, &m_indexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

	// attribute locations 0, 1, and 2 match the vertex shader's inputs
	GLsizei stride = 8 * sizeof(GLfloat);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(GLfloat)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}

/***********************************************************
 *  Draw()
 *
 *  This method is used for drawing the arc with whatever
 *  transformations, colors, and textures are already set in
 *  the shader.
 ***********************************************************/
void BentTubeMesh::Draw() const
{
	if (0 == m_vao)
	{
		return;
	}

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, (void*)0);
	glBindVertexArray(0);
}

/***********************************************************
 *  TriangleCount()
 *
 *  This method is used for getting the number of triangles
 *  Draw() sends to OpenGL.
 ***********************************************************/
int BentTubeMesh::TriangleCount() const
{
	return(m_indexCount / 3);
}

/***********************************************************
 *  Release()
 *
 *  This method is used for freeing the GPU buffers and the
 *  vertex array, if Load() created them.
 ***********************************************************/
void BentTubeMesh::Release()
{
	if (0 != m_vao)
	{
		glDeleteVertexArrays(1, &m_vao);
		glDeleteBuffers(1, &m_vertexBuffer);
		glDeleteBuffers(1, &m_indexBuffer);
	}
	m_vao = 0;
	m_vertexBuffer = 0;
	m_indexBuffer = 0;
	m_indexCount = 0;
}
