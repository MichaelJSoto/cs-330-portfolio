///////////////////////////////////////////////////////////////////////////////
// bentTubeMesh.h
// ============
// build and draw a bent tube: an arc of a torus, swept through any angle
//
// The course torus is always drawn whole or as a half, at 30 by 30 segments
// (1,050 triangles for the half), and its texture coordinates and normals are
// irregular. This mesh is built to order with exact normals and even texture
// coordinates, so a curved part can bend through the angle it needs at a
// low triangle count. It uses the same ring as the course torus, radius 1 in
// the XY plane, starting on +X and turning toward +Y, so a transformation
// written for the course half torus places it the same way.
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <GL/glew.h>

/***********************************************************
 *  BentTubeMesh
 *
 *  This class builds one bent tube in GPU memory and draws
 *  it. Texture coordinates run u from 0 to 1 along the arc
 *  and v from 0 to 1 once around the tube.
 ***********************************************************/
class BentTubeMesh
{
public:
	// constructor
	BentTubeMesh();
	// destructor
	~BentTubeMesh();

	// build the arc: the angle it sweeps, the tube's radius as a fraction
	// of the ring's radius of 1, and the number of segments along the arc
	// and around the tube
	void Load(
		float sweepDegrees,
		float tubeRadius,
		int arcSegments,
		int tubeSegments);

	// draw the arc with the transformations already set in the shader
	void Draw() const;

	// the number of triangles the arc is drawn with
	int TriangleCount() const;

private:
	// free the GPU buffers, if any were built
	void Release();

	GLuint m_vao;
	GLuint m_vertexBuffer;
	GLuint m_indexBuffer;
	GLsizei m_indexCount;
};
