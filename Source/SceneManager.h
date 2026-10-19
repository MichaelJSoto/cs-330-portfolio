///////////////////////////////////////////////////////////////////////////////
// scenemanager.h
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ShaderManager.h"
#include "ShapeMeshes.h"
#include "BentTubeMesh.h"

#include <string>
#include <vector>

/***********************************************************
 *  SceneManager
 *
 *  This class contains the code for preparing and rendering
 *  3D scenes, including the shader settings.
 ***********************************************************/
class SceneManager
{
public:
	// constructor
	SceneManager(ShaderManager *pShaderManager);
	// destructor
	~SceneManager();

	struct TEXTURE_INFO
	{
		std::string tag;
		uint32_t ID;
	};

	struct OBJECT_MATERIAL
	{
		float ambientStrength;
		glm::vec3 ambientColor;
		glm::vec3 diffuseColor;
		glm::vec3 specularColor;
		float shininess;
		std::string tag;
	};

	// the kinds of light the project's fragment shader supports. The
	// values match the LIGHT_ constants in Shaders/fragmentShader.glsl
	enum LightType
	{
		LIGHT_OFF = 0,
		LIGHT_DIRECTIONAL = 1,
		LIGHT_POINT = 2,
		LIGHT_SPOT = 3
	};

	// where the fragment shader takes a surface's normal from. The values
	// match the NORMALS_ constants in Shaders/fragmentShader.glsl
	enum NormalSource
	{
		NORMALS_FROM_MESH = 0,
		NORMALS_CYLINDER_SIDE = 1,
		NORMALS_CONE_SIDE = 2,
		NORMALS_TAPERED_SIDE = 3
	};

	// how the fragment shader makes a surface's texture coordinates. The
	// values match the MAPPING_ constants in Shaders/fragmentShader.glsl
	enum SurfaceMapping
	{
		MAPPING_FROM_MESH = 0,
		MAPPING_CYLINDER_CAP = 1,
		MAPPING_TAPERED_SIDE = 2,
		MAPPING_SPHERE = 3
	};

	// one hardcover book: where the middle of its bottom sits, which way
	// it faces, its size (width across, length front to back, thickness),
	// and the textures and fallback color of its cover and page edges
	struct BOOK_SPEC
	{
		glm::vec3 position;
		float YrotationDegrees;
		float width;
		float length;
		float thickness;
		std::string coverTexture;
		std::string edgeTexture;
		glm::vec3 coverColor;
	};

	struct LIGHT_SOURCE
	{
		LightType type;
		// where a point light or spotlight sits
		glm::vec3 position;
		// the way a directional light or spotlight shines
		glm::vec3 direction;
		glm::vec3 ambientColor;
		glm::vec3 diffuseColor;
		glm::vec3 specularColor;
		float specularIntensity;
		// a spotlight's full-strength cone and the edge of its beam, as
		// angles from its aim in degrees
		float innerConeDegrees;
		float outerConeDegrees;
		// how a point light or spotlight weakens with distance d: its
		// strength is divided by constant + linear * d + quadratic * d * d
		float constant;
		float linear;
		float quadratic;
	};

private:
	// number of texture slots, one per OpenGL texture unit; 16 is the
	// fewest units OpenGL guarantees to a fragment shader
	static const int kMaxTextureSlots = 16;
	// number of light slots in the fragment shader
	static const int kMaxLights = 4;

	// pointer to shader manager object
	ShaderManager* m_pShaderManager;
	// pointer to basic shapes object
	ShapeMeshes* m_basicMeshes;
	// the mug handle, a 180 degree bent tube
	BentTubeMesh m_mugHandleMesh;
	// the lamp's gooseneck, a bent tube that curves over to the shade
	BentTubeMesh m_gooseneckMesh;
	// total number of loaded textures
	int m_loadedTextures;
	// loaded textures info
	TEXTURE_INFO m_textureIDs[kMaxTextureSlots];
	// defined object materials
	std::vector<OBJECT_MATERIAL> m_objectMaterials;
	// the scene's light sources, one per shader light slot
	std::vector<LIGHT_SOURCE> m_sceneLights;
	// true while the desk's objects are drawn mirrored beneath the glass
	bool m_bDrawingReflection;
	// where the object being drawn stands and which way it faces. Each
	// object's parts are placed in the object's own coordinates, and this
	// matrix moves them all to the object's place in the scene
	glm::mat4 m_objectPlacement;

	// load texture images and convert to OpenGL texture data
	bool CreateGLTexture(const char* filename, std::string tag);
	// bind each loaded OpenGL texture to its texture unit
	void BindGLTextures();
	// free the loaded OpenGL textures
	void DestroyGLTextures();
	// find a loaded texture by tag
	int FindTextureID(std::string tag);
	int FindTextureSlot(std::string tag);
	// find a defined material by tag
	bool FindMaterial(std::string tag, OBJECT_MATERIAL& material);

	// build the model matrix from the transformation
	// values and set it into the shader
	void SetTransformations(
		glm::vec3 scaleXYZ,
		float XrotationDegrees,
		float YrotationDegrees,
		float ZrotationDegrees,
		glm::vec3 positionXYZ);

	// set a shape's matrix within its object into the shader: apply the
	// object's placement, mirror it for the reflection, and set the
	// matching normal matrix
	void SetModelMatrix(
		glm::mat4 localMatrix);

	// stand a course mesh that runs up its local Y axis (the cylinder,
	// tapered cylinder, or cone) along the segment from start to end,
	// scaled to the given radius across
	void SetSegmentTransformations(
		glm::vec3 start,
		glm::vec3 end,
		float radius);

	// draw the course box at a size and center within the current
	// object, with the base texture repeating the given number of times
	// along its local x, y, and z
	void DrawBox(
		glm::vec3 size,
		glm::vec3 center,
		glm::vec3 textureRepeats);

	// light the shapes drawn next, or draw them in their own flat color,
	// as a light that glows does
	void SetShaderLighting(
		bool bLit);

	// set the color values into the shader
	void SetShaderColor(
		float redColorValue,
		float greenColorValue,
		float blueColorValue,
		float alphaValue);

	// point the shader's texture sampler at a loaded texture
	void SetShaderTexture(
		std::string textureTag);

	// set the UV scale for the texture mapping
	void SetTextureUVScale(
		float u, float v);

	// lay a second loaded texture over the base texture in the same draw,
	// with its own UV scale, or stop doing so
	void SetShaderOverlay(
		std::string textureTag,
		float u,
		float v);
	void ClearShaderOverlay();

	// map the base texture onto each face of the course box by the face's
	// own position, repeating it the given number of times along the box's
	// local x, y, and z, or go back to the mesh's texture coordinates
	void SetBoxProjection(
		glm::vec3 repeats);
	void ClearBoxProjection();

	// set the UV scale that repeats a square texture a whole number
	// of times around a cylinder's side without stretching it
	void SetCylinderUVScale(
		float radius,
		float height,
		float tilesAround);

	// set the object material into the shader
	void SetShaderMaterial(
		std::string materialTag);

	// set one light source into one of the shader's light slots
	void SetShaderLight(
		int slot,
		const LIGHT_SOURCE& light);

	// set every light slot from m_sceneLights, mirrored beneath the
	// desktop when the reflection is being drawn
	void ApplySceneLights(
		bool bMirrored);

	// tell the shader where to take the next shape's normals from, and
	// whether to turn them inward for the inside of the mug
	void SetShaderNormals(
		NormalSource source,
		bool bInsideSurface);

	// tell the shader how to make the next shape's texture coordinates,
	// with a tapered side's radius at its wide end and its length
	void SetSurfaceMapping(
		SurfaceMapping mapping,
		float taperRadius = 1.0f,
		float taperLength = 1.0f);

	// build the matrix that stands an object at a position on the desk,
	// turned about the vertical axis
	static glm::mat4 PlacementMatrix(
		glm::vec3 position,
		float YrotationDegrees);

	// place the parts drawn next as one object at a position on the desk,
	// turned about the vertical axis, or go back to scene coordinates
	void SetObjectPlacement(
		glm::vec3 position,
		float YrotationDegrees);
	void ClearObjectPlacement();

	// rotate a local offset with the same axis order used by
	// SetTransformations(), so attached parts stay aligned
	glm::vec3 RotateOffset(
		glm::vec3 offset,
		float XrotationDegrees,
		float YrotationDegrees,
		float ZrotationDegrees);

	// load the scene's texture images and bind them to texture units
	void LoadSceneTextures();
	// define the surface materials the scene's shapes are drawn with
	void DefineObjectMaterials();
	// define the scene's light sources and turn the shader's lighting on
	void SetupSceneLights();

	// draw the desk's objects mirrored beneath the glass desktop
	void RenderDeskReflection();
	// set the desktop's transformations and draw its plane
	void DrawDeskPlane();
	// draw every object that stands on the desk
	void RenderDeskObjects();

	// draw each object on the desk, each at its measured place
	void RenderLaptop();
	void RenderBookStack();
	void RenderBook(
		const BOOK_SPEC& book);
	void RenderBallTin();
	void RenderTumbler();
	void RenderTumblerHandle();
	void RenderDeskLamp();
	// draw every part of the mug and the pen
	void RenderMugAndPen();

	// draw each part of the replicated desk scene
	void RenderDeskSurface();
	void RenderMugBody();
	void RenderMugInterior();
	void RenderMugHandle();
	void RenderPen();

public:

	// the following methods are for the students to
	// customize for their own 3D scene
	void PrepareScene();
	void RenderScene();

};