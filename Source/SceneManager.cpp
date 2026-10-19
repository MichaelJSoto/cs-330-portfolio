///////////////////////////////////////////////////////////////////////////////
// scenemanager.cpp
// ============
// manage the loading and rendering of 3D scenes
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//	Created for CS-330-Computational Graphics and Visualization, Nov. 1st, 2023
///////////////////////////////////////////////////////////////////////////////

#include "SceneManager.h"

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include <glm/gtx/transform.hpp>
#include <glm/gtc/constants.hpp>

// declaration of global variables
namespace
{
	const char* g_ModelName = "model";
	const char* g_ColorValueName = "objectColor";
	const char* g_TextureValueName = "objectTexture";
	const char* g_UseTextureName = "bUseTexture";
	const char* g_UseLightingName = "bUseLighting";
	const char* g_NormalMatrixName = "normalMatrix";
	const char* g_NormalSourceName = "normalSource";
	const char* g_FlipNormalsName = "bFlipNormals";
	const char* g_ReflectanceName = "reflectance";
	const char* g_UseOverlayName = "bUseOverlay";
	const char* g_OverlayTextureName = "overlayTexture";
	const char* g_OverlayUVScaleName = "overlayUVscale";
	const char* g_BoxProjectionName = "bBoxProjection";
	const char* g_BoxRepeatsName = "boxRepeats";
	const char* g_SurfaceMappingName = "surfaceMapping";
	const char* g_TaperSizeName = "taperSize";
	const char* g_GlowCoreGainName = "glowCoreGain";

	// the stencil bit that marks the pixels where the glass desktop is
	// visible, so the reflection is drawn there and nowhere else
	const GLuint kGlassStencilBit = 0x80;

	// the share of light the glass desktop reflects when seen straight on.
	// Glass has a refractive index of about 1.5, and a surface with index n
	// reflects ((n - 1) / (n + 1))^2 of the light that strikes it head on,
	// which for glass is (0.5 / 2.5)^2 = 0.04
	const float kGlassReflectance = 0.04f;
}

// dimensions of the replicated UCONN Health mug and its pen.
// The proportions come from the Milestone One photos: IMG_0057
// is the low, edge-on frame and IMG_0058 is the overhead frame.
// Pixel sizes below count both end pixels, and offsets between
// two points are differences. Where an edge pixel is blended or
// a glaze highlight sits on an edge, a range covers both
// readings, and values that could not be isolated in the photos
// are marked as estimates or choices. Every size is a fraction
// of the mug's diameter or height, so the whole object rescales
// from kMugRadius alone
namespace
{
	// overall size: the mug is 4 units across, the unit the whole scene's
	// layout is measured in
	const float kMugRadius = 2.0f;
	const float kMugDiameter = 2.0f * kMugRadius;

	// IMG_0057: on rows 960 and 980, where the handle opening shows
	// beside the body, the body is 203 to 211 px wide, depending on
	// whether the glaze highlight on its right edge is counted. Its
	// bottom edge merges with its reflection in the black glass, so
	// the height is an estimate taken from a zoomed view. (Fitting the
	// rim in IMG_0055 and IMG_0058 together puts it at 0.88 diameters;
	// 0.85 is kept because the lettering was measured against it)
	const float kMugHeight = 0.85f * kMugDiameter;
	// the rim is too thin to measure, so this thickness is a choice
	const float kMugWallThickness = 0.04f * kMugDiameter;

	// IMG_0057: the handle reaches 81 to 91 px past the body. On rows
	// 960 to 990 its band is 19 to 23 px across from the opening to
	// the background, of which 12 to 16 px is its dark core. It is
	// 111 to 140 px tall, depending on the column and on where its
	// blended top edge is placed. The tube here is sized to the dark
	// core, and the height used, about two thirds of the body width,
	// is near the top of that range. Its middle sitting just below
	// half the body height is an estimate, since the bottom of the
	// body cannot be isolated in this frame
	const float kHandleReach = 0.42f * kMugDiameter;
	const float kHandleHalfSpan = 0.33f * kMugDiameter;
	const float kHandleTubeRadius = 0.032f * kMugDiameter;
	const float kHandleCenterHeight = 0.47f * kMugHeight;
	// IMG_0058: from above, the handle band is 49 to 51 px deep near
	// the body and narrows to 40 to 42 px, then 25 to 27 px, toward
	// its rounded end, against a rim about 212 px across. The depth
	// used here, about a fifth of the rim width, matches the middle
	// of that taper
	const float kHandleHalfDepth = 0.10f * kMugDiameter;
	// the bent tube mesh sets its tube radius as a fraction of its
	// ring radius, and the handle's ring radius is its reach
	const float kHandleTubeFraction = kHandleTubeRadius / kHandleReach;
	// the handle's segments along its arc and around its tube: 384
	// triangles, which keeps the mug and pen together under 1,000 (the
	// course half torus alone drew 1,050)
	const int kHandleArcSegments = 16;
	const int kHandleTubeSegments = 12;

	// IMG_0057: the top of the pen is 63 to 64 px left of and 93 to
	// 96 px above the point where it disappears behind the front of
	// the rim. IMG_0058: the visible pen points about 70 px left for
	// every 21 px toward the back of the desk, a swing of about 17
	// degrees. Undoing that swing from the side view's slope gives a
	// lean of about 35 degrees away from vertical
	const float kPenTiltDegrees = 35.0f;
	const float kPenYawDegrees = -17.0f;
	// estimate: where the pen crosses the rim cannot be seen from
	// directly above, so this places it left of and slightly behind
	// the center of the opening, the part of the opening the
	// visible pen covers in IMG_0058
	const float kPenEntryX = -0.21f * kMugDiameter;
	const float kPenEntryZ = -0.06f * kMugDiameter;
	// the side view's rise, with the 17 degree swing restored, puts a
	// little over half a diameter of pen above the rim
	const float kPenLengthAboveRim = 0.56f * kMugDiameter;
	// the pen is too thin to measure reliably, so these are choices
	const float kPenRadius = 0.02f * kMugDiameter;
	const float kPenTipLength = 0.06f * kMugDiameter;

	// the glaze image repeats a whole number of times around the mug, so
	// its right edge meets its left edge where the cylinder mesh wraps
	// from u = 1 back to u = 0, on the +X side behind the handle, just as
	// the edges of neighboring repeats meet everywhere else. Four repeats
	// make each one a square 3.14 units wide
	const float kGlazeTilesAround = 4.0f;
	const float kGlazeTileWidth = glm::two_pi<float>() * kMugRadius / kGlazeTilesAround;
}

// the scene's layout, measured from the Milestone One photos. Each
// camera was solved from the desk's traced edges in IMG_0055 (from the
// front), IMG_0058 (from overhead), and IMG_0060 (from behind), and each
// object was fitted to its outline in two or three of those frames. The
// mug, 4 units across, sets the scale. x runs right along the front of
// the desk, y up, and z toward the front; the origin is the middle of
// the desktop
namespace
{
	// the glass desktop, 7.03 mug diameters wide and 1.68 times as wide as
	// it is deep
	const float kDeskWidth = 28.12f;
	const float kDeskDepth = 16.74f;
	// the glass texture repeats every 10 units, so its texels stay square
	const float kDeskTileSize = 10.0f;

	// the middle of the mug's base, right of the laptop and a little in
	// front of the middle of the desk's depth
	const glm::vec3 kMugPosition = glm::vec3(10.27f, 0.0f, 0.73f);

	// the closed laptop: the middle of its footprint, its turn, and its
	// size. Its top was fitted in all three frames at 0.86 units
	const glm::vec3 kLaptopPosition = glm::vec3(0.18f, 0.0f, -0.55f);
	const float kLaptopYawDegrees = 0.84f;
	const float kLaptopWidth = 11.59f;
	const float kLaptopDepth = 8.37f;
	const float kLaptopHeight = 0.86f;

	// the two books lie squarely on top of each other, long sides running
	// front to back and turned 21 degrees; the ball tin sits on top of the
	// stack at 2.59 units, where its sphere fit ends
	const glm::vec3 kBookStackPosition = glm::vec3(-10.59f, 0.0f, 2.56f);
	const float kBookYawDegrees = 21.18f;
	const float kBookWidth = 5.45f;
	const float kBookLength = 8.23f;
	const float kBookThickness = 1.293f;

	// the ball tin: a sphere fitted to its outline in all three frames. Its
	// image draws the lid seam 11 degrees above its middle, and these turns
	// about Z and then X lay that seam on the one traced in IMG_0055 (0.7 px
	// rms; an estimate, since only the seam's front right shows)
	const glm::vec3 kTinCenter = glm::vec3(-10.50f, 4.10f, 2.91f);
	const float kTinRadius = 1.509f;
	const float kTinTipXDegrees = -2.1f;
	const float kTinTipZDegrees = -3.8f;

	// the tumbler: the middle of its base, turned so its handle faces the
	// way it does from above in IMG_0058
	const glm::vec3 kTumblerPosition = glm::vec3(8.97f, 0.0f, -5.88f);
	const float kTumblerYawDegrees = -14.2f;

	// the desk lamp: the middle of its base
	const glm::vec3 kLampPosition = glm::vec3(-11.02f, 0.0f, -4.73f);
}

// sizes of the laptop, books, tumbler, and lamp, in scene units
namespace
{
	// the laptop's front face, which IMG_0055 shows, is 0.40 units tall:
	// the lid on top, with the seam a third of the way down, and the upper
	// base under it. The base's lower part, which no photo shows, is set
	// in so the overhang hides most of it from the starting view
	const float kLaptopLidThickness = 0.13f;
	const float kLaptopUpperBase = 0.27f;
	const float kLaptopUndersideInset = 0.6f;
	// brushed aluminum repeats every 4 units
	const float kAluminumTile = 4.0f;

	// a book's cover boards and spine, and how far the boards reach past
	// the page block (choices: the boards are too thin to measure)
	const float kBookBoard = 0.08f;
	const float kBookOverhang = 0.12f;
	// the printed page edges repeat every 2.5 units
	const float kBookEdgeTile = 2.5f;

	// the tumbler, from IMG_0055's outline converted to radius and height
	// about its axis: a narrow base, a short taper, the body, a steel band,
	// and the lid
	const float kTumblerBaseRadius = 1.42f;
	const float kTumblerTaperBottom = 2.8f;
	const float kTumblerTaperTop = 3.6f;
	const float kTumblerBodyRadius = 1.72f;
	const float kTumblerBandBottom = 7.87f;
	const float kTumblerBandTop = 8.47f;
	const float kTumblerLidRadius = 1.74f;
	const float kTumblerLidTop = 9.24f;
	// the handle's loop, in the plane through the axis (IMG_0055 readings,
	// estimates): pivots on the lid at 8.98, a top bar at 11.43, and a strap
	// 0.16 thick and 0.45 wide. The sides rise to 9.9, then slant in to the
	// bar; the slants pass through the photo's left upper corner (10.35
	// high) and 0.36 below its right one (10.91)
	const float kHandlePivotOut = 1.87f;
	const float kHandlePivotHeight = 8.98f;
	const float kHandleSideTop = 9.9f;
	const float kHandleBarHalfLength = 1.0f;
	const float kHandleBarHeight = 11.43f;
	const float kHandleStrapThickness = 0.16f;
	const float kHandleStrapWidth = 0.45f;

	// the lamp, in its own coordinates from the middle of its base. Its
	// points were triangulated across the frames and are estimates: the
	// base from IMG_0060's ellipse; the stem foot (lowered to the base's
	// top) and the start of the ribbed gooseneck from IMG_0055 and
	// IMG_0060; the shade's end cap from all three frames and its mouth
	// from IMG_0055 and IMG_0058. The shade was moved 0.70 units toward the
	// gooseneck's measured top so the two meet at the shade's underside
	const float kLampBaseRadius = 2.57f;
	const float kLampBaseHeight = 0.74f;
	const glm::vec3 kLampStemFoot = glm::vec3(-1.54f, 0.74f, -1.49f);
	const glm::vec3 kLampStemTop = glm::vec3(-1.36f, 4.76f, -1.60f);
	const float kLampTubeRadius = 0.27f;
	// the gooseneck rises straight from the stem's top and bends toward
	// the shade: one arc of radius 9.05, turning 49.6 degrees in the
	// vertical plane that faces this way
	const glm::vec3 kGooseneckBendDirection = glm::vec3(0.8163f, 0.0f, 0.5777f);
	const float kGooseneckBendRadius = 9.045f;
	const float kGooseneckSweepDegrees = 49.6f;
	// about 0.11 units from rib to rib (IMG_0060), 16 ribs to a texture tile
	const float kGooseneckRibTile = 16.0f * 0.11f;
	const glm::vec3 kShadeEndCap = glm::vec3(0.681f, 13.749f, -0.531f);
	const glm::vec3 kShadeMouthCenter = glm::vec3(3.921f, 9.919f, 2.009f);
	const float kShadeHousingRadius = 0.95f;
	const float kShadeHousingLength = 3.0f;
	const float kShadeMouthRadius = 2.32f;
	// the shade's axis from its back end to its mouth, the bell's length
	// from the neck to the mouth, and the bulb, where the bell meets the neck
	const glm::vec3 kShadeAxis = glm::normalize(kShadeMouthCenter - kShadeEndCap);
	const float kShadeBellLength = glm::length(kShadeMouthCenter - kShadeEndCap) - kShadeHousingLength;
	const glm::vec3 kLampBulb = kShadeMouthCenter - kShadeAxis * kShadeBellLength;
	// the lamp's points were measured along the scene's own axes, so the
	// lamp stands unturned
	const float kLampYawDegrees = 0.0f;
	// a 2700 K incandescent bulb, from Mitchell Charity's blackbody color
	// table (CIE 1964 10 degree row): 255, 169, 87. The shader mixes colors
	// as the images store them, gamma-encoded, so the table's 8-bit value is
	// used rather than its linear one
	const glm::vec3 kLampColor = glm::vec3(255.0f, 169.0f, 87.0f) / 255.0f;
	// the middle of the lit mouth shows the bulb, far brighter than anything
	// it lights, so it glows at twice this color, each channel stopping at
	// 1: 255, 255, 174. A choice, since the lamp is off in every photo
	const float kLampGlowCoreGain = 2.0f;
}

/***********************************************************
 *  SceneManager()
 *
 *  The constructor for the class.
 ***********************************************************/
SceneManager::SceneManager(ShaderManager *pShaderManager)
{
	m_pShaderManager = pShaderManager;
	m_basicMeshes = new ShapeMeshes();

	// start with every texture slot empty. CreateGLTexture() fills the
	// slots in order and counts them in m_loadedTextures, so the count
	// must start at 0 before the first texture is loaded
	for (int i = 0; i < kMaxTextureSlots; i++)
	{
		m_textureIDs[i].tag = "";
		m_textureIDs[i].ID = 0;
	}
	m_loadedTextures = 0;

	// the scene is drawn the right way up until RenderDeskReflection()
	// draws its mirror image
	m_bDrawingReflection = false;

	// shapes are placed in scene coordinates until an object sets its own
	// placement
	m_objectPlacement = glm::mat4(1.0f);
}

/***********************************************************
 *  ~SceneManager()
 *
 *  The destructor for the class.
 ***********************************************************/
SceneManager::~SceneManager()
{
	// free the OpenGL textures while the OpenGL context that owns them
	// still exists; main() deletes this object before the program exits
	DestroyGLTextures();

	m_pShaderManager = NULL;
	delete m_basicMeshes;
	m_basicMeshes = NULL;
}

/***********************************************************
 *  CreateGLTexture()
 *
 *  This method is used for loading textures from image files,
 *  configuring the texture mapping parameters in OpenGL,
 *  generating the mipmaps, and loading the read texture into
 *  the next available texture slot in memory.
 ***********************************************************/
bool SceneManager::CreateGLTexture(const char* filename, std::string tag)
{
	int width = 0;
	int height = 0;
	int colorChannels = 0;
	GLuint textureID = 0;

	// m_textureIDs has a fixed number of slots, so stop before reading
	// the image once they are all in use, rather than writing past the
	// end of the array
	if (m_loadedTextures >= kMaxTextureSlots)
	{
		std::cout << "Could not load image:" << filename << ", all " << kMaxTextureSlots << " texture slots are in use" << std::endl;
		return false;
	}

	// indicate to always flip images vertically when loaded
	stbi_set_flip_vertically_on_load(true);

	// try to parse the image data from the specified image file
	unsigned char* image = stbi_load(
		filename,
		&width,
		&height,
		&colorChannels,
		0);

	// if the image was successfully read from the image file
	if (image)
	{
		std::cout << "Successfully loaded image:" << filename << ", width:" << width << ", height:" << height << ", channels:" << colorChannels << std::endl;

		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// set the texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// set texture filtering parameters. When the texture is drawn
		// smaller than its image, the minification filter blends the two
		// mipmap levels (built below) nearest to the drawn size, so
		// fine detail such as the glaze flecks is averaged instead of
		// being sampled from texels far apart, which breaks it into
		// pixel noise
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// if the loaded image is in RGB format
		if (colorChannels == 3)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
		// if the loaded image is in RGBA format, which supports transparency
		else if (colorChannels == 4)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
		else
		{
			std::cout << "Not implemented to handle image with " << colorChannels << " channels" << std::endl;
			// release the image data and the texture name created above
			stbi_image_free(image);
			glBindTexture(GL_TEXTURE_2D, 0);
			glDeleteTextures(1, &textureID);
			return false;
		}

		// generate the texture mipmaps, the smaller copies of the texture
		// that the minification filter set above draws from
		glGenerateMipmap(GL_TEXTURE_2D);

		// free the image data from local memory
		stbi_image_free(image);
		glBindTexture(GL_TEXTURE_2D, 0); // unbind the texture

		// register the loaded texture and associate it with its tag string
		m_textureIDs[m_loadedTextures].ID = textureID;
		m_textureIDs[m_loadedTextures].tag = tag;
		m_loadedTextures++;

		return true;
	}

	std::cout << "Could not load image:" << filename << std::endl;

	// error loading the image
	return false;
}

/***********************************************************
 *  BindGLTextures()
 *
 *  This method is used for binding each loaded texture to
 *  the OpenGL texture unit that matches its index in
 *  m_textureIDs, which holds up to kMaxTextureSlots (16)
 *  textures. CreateGLTexture() refuses any texture past
 *  that limit.
 ***********************************************************/
void SceneManager::BindGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		// bind textures on corresponding texture units
		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, m_textureIDs[i].ID);
	}
}

/***********************************************************
 *  DestroyGLTextures()
 *
 *  This method is used for freeing the OpenGL memory of all
 *  the loaded textures and emptying their slots. The course
 *  starter called glGenTextures() here, which creates new
 *  texture names and frees nothing; glDeleteTextures() is
 *  the call that frees a texture.
 ***********************************************************/
void SceneManager::DestroyGLTextures()
{
	for (int i = 0; i < m_loadedTextures; i++)
	{
		glDeleteTextures(1, &m_textureIDs[i].ID);
		m_textureIDs[i].ID = 0;
		m_textureIDs[i].tag = "";
	}
	m_loadedTextures = 0;
}

/***********************************************************
 *  FindTextureID()
 *
 *  This method is used for getting an ID for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureID(std::string tag)
{
	int textureID = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureID = m_textureIDs[index].ID;
			bFound = true;
		}
		else
			index++;
	}

	return(textureID);
}

/***********************************************************
 *  FindTextureSlot()
 *
 *  This method is used for getting a slot index for the previously
 *  loaded texture bitmap associated with the passed in tag.
 ***********************************************************/
int SceneManager::FindTextureSlot(std::string tag)
{
	int textureSlot = -1;
	int index = 0;
	bool bFound = false;

	while ((index < m_loadedTextures) && (bFound == false))
	{
		if (m_textureIDs[index].tag.compare(tag) == 0)
		{
			textureSlot = index;
			bFound = true;
		}
		else
			index++;
	}

	return(textureSlot);
}

/***********************************************************
 *  FindMaterial()
 *
 *  This method is used for getting a material from the previously
 *  defined materials list that is associated with the passed in tag.
 ***********************************************************/
bool SceneManager::FindMaterial(std::string tag, OBJECT_MATERIAL& material)
{
	if (m_objectMaterials.size() == 0)
	{
		return(false);
	}

	int index = 0;
	bool bFound = false;
	while ((index < m_objectMaterials.size()) && (bFound == false))
	{
		if (m_objectMaterials[index].tag.compare(tag) == 0)
		{
			bFound = true;
			material.ambientColor = m_objectMaterials[index].ambientColor;
			material.ambientStrength = m_objectMaterials[index].ambientStrength;
			material.diffuseColor = m_objectMaterials[index].diffuseColor;
			material.specularColor = m_objectMaterials[index].specularColor;
			material.shininess = m_objectMaterials[index].shininess;
		}
		else
		{
			index++;
		}
	}

	return(true);
}

/***********************************************************
 *  SetTransformations()
 *
 *  This method is used for building the model matrix from
 *  the passed in scale, rotation, and position values and
 *  setting it into the shader's model uniform, along with
 *  the matrix that turns the shape's normals. The values
 *  place the shape within the object being drawn, and the
 *  object's placement (SetObjectPlacement()) then moves it
 *  into the scene. While the reflection is drawn, the shape
 *  is also mirrored through the desktop at y = 0.
 ***********************************************************/
void SceneManager::SetTransformations(
	glm::vec3 scaleXYZ,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees,
	glm::vec3 positionXYZ)
{
	// variables for this method
	glm::mat4 modelView;
	glm::mat4 scale;
	glm::mat4 rotationX;
	glm::mat4 rotationY;
	glm::mat4 rotationZ;
	glm::mat4 translation;

	// build the scale matrix
	scale = glm::scale(scaleXYZ);
	// build a rotation matrix for each axis
	rotationX = glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f));
	rotationY = glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f));
	rotationZ = glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
	// build the translation matrix
	translation = glm::translate(positionXYZ);

	modelView = translation * rotationX * rotationY * rotationZ * scale;

	SetModelMatrix(modelView);
}

/***********************************************************
 *  SetModelMatrix()
 *
 *  This method is used for setting a shape's matrix within
 *  its object into the shader. It applies the object's
 *  placement (SetObjectPlacement()), mirrors the shape
 *  through the desktop at y = 0 while the reflection is
 *  drawn, and sets the matrix that turns the shape's normals.
 ***********************************************************/
void SceneManager::SetModelMatrix(
	glm::mat4 localMatrix)
{
	glm::mat4 model = m_objectPlacement * localMatrix;

	// the reflection is the shape's mirror image below the glass, made by
	// flipping the finished model matrix's heights
	if (m_bDrawingReflection)
	{
		model = glm::scale(glm::vec3(1.0f, -1.0f, 1.0f)) * model;
	}

	// normals have to stay at right angles to the surface. The inverse
	// transpose of the model matrix's rotation and scale turns them with
	// the shape, keeps them square to it where it is stretched more along
	// one axis than another (as the handle and pen are), and flips them
	// with the mirror image
	glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setMat4Value(g_ModelName, model);
		m_pShaderManager->setMat3Value(g_NormalMatrixName, normalMatrix);
	}
}

/***********************************************************
 *  SetSegmentTransformations()
 *
 *  This method is used for standing a course mesh that runs
 *  up its local Y axis, from y = 0 to y = 1 (the cylinder,
 *  tapered cylinder, or cone), along the segment from start
 *  to end within the current object, scaled to the radius
 *  across. The meshes are round, so any direction square to
 *  the segment serves as their local X.
 ***********************************************************/
void SceneManager::SetSegmentTransformations(
	glm::vec3 start,
	glm::vec3 end,
	float radius)
{
	glm::vec3 axis = end - start;
	float length = glm::length(axis);
	glm::vec3 up = axis / length;

	glm::vec3 side = glm::vec3(1.0f, 0.0f, 0.0f);
	if (glm::abs(up.y) < 0.99f)
	{
		side = glm::normalize(glm::cross(up, glm::vec3(0.0f, 1.0f, 0.0f)));
	}
	glm::vec3 front = glm::cross(side, up);

	// the columns are where the mesh's X, Y, and Z axes and origin land
	glm::mat4 basis(
		glm::vec4(side, 0.0f),
		glm::vec4(up, 0.0f),
		glm::vec4(front, 0.0f),
		glm::vec4(start, 1.0f));

	SetModelMatrix(basis * glm::scale(glm::vec3(radius, length, radius)));
}

/***********************************************************
 *  DrawBox()
 *
 *  This method is used for drawing the course box at a size
 *  and center within the current object, with the texture
 *  laid onto each face by the face's own position, repeating
 *  the given number of times along the box's local x, y, and
 *  z. A box's size divided by a texture's tile size gives
 *  repeats that keep the texture's texels square.
 ***********************************************************/
void SceneManager::DrawBox(
	glm::vec3 size,
	glm::vec3 center,
	glm::vec3 textureRepeats)
{
	SetTransformations(size, 0.0f, 0.0f, 0.0f, center);
	SetShaderNormals(NORMALS_FROM_MESH, false);
	SetBoxProjection(textureRepeats);
	m_basicMeshes->DrawBoxMesh();
	ClearBoxProjection();
}

/***********************************************************
 *  SetShaderLighting()
 *
 *  This method is used for turning the shader's lighting on
 *  or off for the shapes drawn next. A part that glows, such
 *  as the laptop's power light or the lamp's lit mouth, is
 *  drawn unlit in its own flat color.
 ***********************************************************/
void SceneManager::SetShaderLighting(
	bool bLit)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setBoolValue(g_UseLightingName, bLit);
	}
}

/***********************************************************
 *  SetShaderColor()
 *
 *  This method is used for setting the passed in color
 *  into the shader for the next draw command, with
 *  texturing off until SetShaderTexture() turns it back on.
 ***********************************************************/
void SceneManager::SetShaderColor(
	float redColorValue,
	float greenColorValue,
	float blueColorValue,
	float alphaValue)
{
	// variables for this method
	glm::vec4 currentColor;

	currentColor.r = redColorValue;
	currentColor.g = greenColorValue;
	currentColor.b = blueColorValue;
	currentColor.a = alphaValue;

	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_UseTextureName, false);
		m_pShaderManager->setVec4Value(g_ColorValueName, currentColor);
	}
}

/***********************************************************
 *  SetShaderTexture()
 *
 *  This method is used for turning on texturing in the
 *  shader and pointing its texture sampler at the texture
 *  unit that holds the texture loaded with the passed in tag.
 *
 *  If no texture has that tag, for example because its image
 *  file was missing, the shader is left as it was, so the
 *  next shape shows the color set by SetShaderColor() before
 *  this call. The course starter passed the -1 that
 *  FindTextureSlot() returns to the sampler, which OpenGL
 *  rejects, and the shape then took on whichever texture
 *  the previous shape had used.
 ***********************************************************/
void SceneManager::SetShaderTexture(
	std::string textureTag)
{
	if (NULL != m_pShaderManager)
	{
		int textureSlot = FindTextureSlot(textureTag);

		if (textureSlot >= 0)
		{
			m_pShaderManager->setIntValue(g_UseTextureName, true);
			m_pShaderManager->setSampler2DValue(g_TextureValueName, textureSlot);
		}
	}
}

/***********************************************************
 *  SetTextureUVScale()
 *
 *  This method is used for setting the texture UV scale
 *  values into the shader.
 ***********************************************************/
void SceneManager::SetTextureUVScale(float u, float v)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setVec2Value("UVscale", glm::vec2(u, v));
	}
}

/***********************************************************
 *  SetShaderOverlay()
 *
 *  This method is used for laying a second loaded texture
 *  over the base texture in the same draw. The fragment
 *  shader samples both and covers the base with the overlay
 *  in proportion to the overlay's alpha, so a transparent
 *  background leaves the base showing. The overlay has its
 *  own UV scale. An unknown tag leaves the overlay off.
 ***********************************************************/
void SceneManager::SetShaderOverlay(
	std::string textureTag,
	float u,
	float v)
{
	if (NULL == m_pShaderManager)
	{
		return;
	}

	int textureSlot = FindTextureSlot(textureTag);
	m_pShaderManager->setBoolValue(g_UseOverlayName, textureSlot >= 0);
	if (textureSlot >= 0)
	{
		m_pShaderManager->setSampler2DValue(g_OverlayTextureName, textureSlot);
		m_pShaderManager->setVec2Value(g_OverlayUVScaleName, glm::vec2(u, v));
	}
}

/***********************************************************
 *  ClearShaderOverlay()
 *
 *  This method is used for turning the overlay texture off,
 *  so the shapes drawn next show their base texture alone.
 ***********************************************************/
void SceneManager::ClearShaderOverlay()
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setBoolValue(g_UseOverlayName, false);
	}
}

/***********************************************************
 *  SetBoxProjection()
 *
 *  This method is used for mapping the base texture onto
 *  each face of the course box by the face's own position
 *  rather than by the mesh's texture coordinates, which
 *  stretch the whole image over every face. The image
 *  repeats the given number of times along the box's local
 *  x, y, and z, so a box scaled to any size keeps square
 *  texels when the repeats are its size over the tile size.
 ***********************************************************/
void SceneManager::SetBoxProjection(
	glm::vec3 repeats)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setBoolValue(g_BoxProjectionName, true);
		m_pShaderManager->setVec3Value(g_BoxRepeatsName, repeats);
	}
}

/***********************************************************
 *  ClearBoxProjection()
 *
 *  This method is used for returning the shader to the
 *  mesh's own texture coordinates.
 ***********************************************************/
void SceneManager::ClearBoxProjection()
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setBoolValue(g_BoxProjectionName, false);
	}
}

/***********************************************************
 *  SetCylinderUVScale()
 *
 *  This method is used for setting the UV scale that spreads
 *  a square texture over the side of a cylinder without
 *  stretching it. The cylinder mesh runs u once around its
 *  side and v once from bottom to top, so an unscaled image
 *  covers a surface 2 pi r wide and h tall. The image repeats
 *  a whole number of times around, so its edges also meet
 *  cleanly where u wraps from 1 back to 0, and the repeats up
 *  the side are counted so each one covers a square patch,
 *  the same shape as the image.
 ***********************************************************/
void SceneManager::SetCylinderUVScale(
	float radius,
	float height,
	float tilesAround)
{
	// one repeat is the circumference divided by the number around,
	// and that many units fit into the height this many times
	float tilesUp = tilesAround * height / (glm::two_pi<float>() * radius);

	SetTextureUVScale(tilesAround, tilesUp);
}

/***********************************************************
 *  SetShaderMaterial()
 *
 *  This method is used for passing the material values
 *  into the shader.
 ***********************************************************/
void SceneManager::SetShaderMaterial(
	std::string materialTag)
{
	if (m_objectMaterials.size() > 0)
	{
		OBJECT_MATERIAL material;
		bool bReturn = false;

		bReturn = FindMaterial(materialTag, material);
		if (bReturn == true)
		{
			m_pShaderManager->setVec3Value("material.ambientColor", material.ambientColor);
			m_pShaderManager->setFloatValue("material.ambientStrength", material.ambientStrength);
			m_pShaderManager->setVec3Value("material.diffuseColor", material.diffuseColor);
			m_pShaderManager->setVec3Value("material.specularColor", material.specularColor);
			m_pShaderManager->setFloatValue("material.shininess", material.shininess);
		}
	}
}

/***********************************************************
 *  SetShaderLight()
 *
 *  This method is used for passing one light source's values
 *  into one of the shader's light slots. The cone angles are
 *  passed as cosines, which the shader compares directly
 *  with the cosine of the angle to each fragment.
 ***********************************************************/
void SceneManager::SetShaderLight(
	int slot,
	const LIGHT_SOURCE& light)
{
	if (NULL == m_pShaderManager)
	{
		return;
	}

	std::string name = "lightSources[" + std::to_string(slot) + "].";

	m_pShaderManager->setIntValue(name + "type", light.type);
	m_pShaderManager->setVec3Value(name + "position", light.position);
	m_pShaderManager->setVec3Value(name + "direction", light.direction);
	m_pShaderManager->setVec3Value(name + "ambientColor", light.ambientColor);
	m_pShaderManager->setVec3Value(name + "diffuseColor", light.diffuseColor);
	m_pShaderManager->setVec3Value(name + "specularColor", light.specularColor);
	m_pShaderManager->setFloatValue(name + "specularIntensity", light.specularIntensity);
	m_pShaderManager->setFloatValue(name + "innerConeCos",
		glm::cos(glm::radians(light.innerConeDegrees)));
	m_pShaderManager->setFloatValue(name + "outerConeCos",
		glm::cos(glm::radians(light.outerConeDegrees)));
	m_pShaderManager->setFloatValue(name + "constant", light.constant);
	m_pShaderManager->setFloatValue(name + "linear", light.linear);
	m_pShaderManager->setFloatValue(name + "quadratic", light.quadratic);
}

/***********************************************************
 *  ApplySceneLights()
 *
 *  This method is used for passing every light in
 *  m_sceneLights into the shader, and switching off any
 *  slot left over. For the reflection, each light is moved
 *  to its mirror image below the desktop, so the mirrored
 *  mug is lit from the same side as the real one, the way
 *  a mirror shows it.
 ***********************************************************/
void SceneManager::ApplySceneLights(
	bool bMirrored)
{
	for (int slot = 0; slot < kMaxLights; slot++)
	{
		LIGHT_SOURCE light = {};
		light.type = LIGHT_OFF;

		if (slot < static_cast<int>(m_sceneLights.size()))
		{
			light = m_sceneLights[slot];
		}

		if (bMirrored)
		{
			light.position.y = -light.position.y;
			light.direction.y = -light.direction.y;
		}

		SetShaderLight(slot, light);
	}
}

/***********************************************************
 *  SetShaderNormals()
 *
 *  This method is used for telling the shader where to take
 *  the next shape's normals from: the mesh itself, or the
 *  exact normal of the course cylinder's, cone's, or
 *  tapered cylinder's side (the shader explains why those
 *  are rebuilt).
 *  The inside of the mug is drawn with meshes whose normals
 *  point outward, so bInsideSurface turns them inward.
 ***********************************************************/
void SceneManager::SetShaderNormals(
	NormalSource source,
	bool bInsideSurface)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_NormalSourceName, source);
		m_pShaderManager->setBoolValue(g_FlipNormalsName, bInsideSurface);
	}
}

/***********************************************************
 *  SetSurfaceMapping()
 *
 *  This method is used for telling the shader how to make
 *  the next shape's texture coordinates: from the mesh, or
 *  from the surface position for a course cylinder's cap,
 *  a tapered cylinder's side, or the sphere, whose stored
 *  coordinates stretch or skew the image (the shader
 *  explains each layout). A tapered side also needs its
 *  radius at its wide end and its length.
 ***********************************************************/
void SceneManager::SetSurfaceMapping(
	SurfaceMapping mapping,
	float taperRadius,
	float taperLength)
{
	if (NULL != m_pShaderManager)
	{
		m_pShaderManager->setIntValue(g_SurfaceMappingName, mapping);
		m_pShaderManager->setVec2Value(g_TaperSizeName, glm::vec2(taperRadius, taperLength));
	}
}

/**************************************************************/
/*** STUDENTS CAN MODIFY the code in the methods BELOW for  ***/
/*** preparing and rendering their own 3D replicated scenes.***/
/*** Please refer to the code in the OpenGL sample project  ***/
/*** for assistance.                                        ***/
/**************************************************************/


/***********************************************************
 *  LoadSceneTextures()
 *
 *  This method is used for loading the image files that the
 *  3D scene uses as textures into OpenGL texture memory and
 *  binding them to texture units. PrepareScene() calls it
 *  once, before the meshes are loaded.
 ***********************************************************/
void SceneManager::LoadSceneTextures()
{
	// each CreateGLTexture() call reads one image file and registers it
	// under a tag string, which the render methods pass to
	// SetShaderTexture() or, for the lettering, SetShaderOverlay().
	// CreateGLTexture() prints a console line for each image it loads or
	// fails to load, and a shape whose texture failed keeps its flat
	// color, so the return values are not needed here. Every image was
	// made for this project, from measurements of the Milestone One
	// photos where the photos show the detail, so none carries a license
	// restriction. They ship in this project's Textures folder and load
	// relative to the project folder, the working directory under Visual
	// Studio and, through SetWorkingDirectoryToProject() in MainCode.cpp,
	// when the EXE is started directly. Every image is at least 1024
	// pixels on each side. The three glaze images, the barrel, and the
	// desk each average the flat color their part had in Milestone Two to
	// within one level per channel. The other objects' images are built on
	// colors taken from samples in the photos, several of them brightened

	// black glass for the desktop, with faint smudges, wipe marks, and
	// dust, which RenderDeskSurface() tiles across the desk. The photos
	// show the glass only through its reflections, so this surface detail
	// is a choice rather than a measurement
	CreateGLTexture(
		"Textures/desk_glass.jpg",
		"desk_glass");

	// navy enamel with white flecks, for the outer wall. In IMG_0056 the
	// 5.4 square units of wall right of the handle hold 74 to 98 flecks,
	// depending on the detection threshold, covering 2.2 to 4.6 percent
	// of it. The flecks are streaks rather than dots, a median of 1.7
	// times longer than wide. This tile holds 157 flecks in that size
	// range, covering 3.2 percent
	CreateGLTexture(
		"Textures/mug_glaze.jpg",
		"mug_glaze");

	// the same glaze for the handle, with its flecks drawn 1.81 times
	// taller and 1.81 times fewer, because the handle's tube squeezes the
	// image that much across the band (RenderMugHandle() explains why).
	// On the handle they come out at the body's density, covering 3.7
	// percent against the body's 3.2
	CreateGLTexture(
		"Textures/mug_glaze_handle.jpg",
		"mug_glaze_handle");

	// the same glaze darkened to the Milestone Two interior navy, for the
	// inside wall and floor. The lit back of the inside wall in IMG_0056
	// still shows flecks (8 in a 130 by 50 px patch), so they stay,
	// dimmed along with the glaze
	CreateGLTexture(
		"Textures/mug_glaze_interior.jpg",
		"mug_glaze_interior");

	// the UCONN HEALTH print on a transparent background, drawn over the
	// glaze by RenderMugBody(), which describes its layout
	CreateGLTexture(
		"Textures/mug_lettering.png",
		"mug_lettering");

	// teal plastic for the pen barrel, with a faint variation (a 2
	// percent standard deviation) so it does not render as a dead flat
	// color. The barrel is only about 7 to 16 px wide in the photos, too
	// narrow to show any pattern of its own
	CreateGLTexture(
		"Textures/pen_barrel.jpg",
		"pen_barrel");

	// the pen tip, laid out as rings that RenderPen() maps onto the cone
	CreateGLTexture(
		"Textures/pen_tip.jpg",
		"pen_tip");

	// brushed aluminum for the laptop, also used for the tumbler's steel
	// band and the lamp's smooth stem; its streaks run along u
	CreateGLTexture(
		"Textures/laptop_aluminum.jpg",
		"laptop_aluminum");

	// the two books' cloth covers, with roses and hearts drawn for this
	// project rather than copied from the published covers. Each is
	// 1024x1546, the shape of the measured cover, so it maps once across
	// the cover without stretching
	CreateGLTexture(
		"Textures/book_cover_blue.jpg",
		"book_cover_blue");
	CreateGLTexture(
		"Textures/book_cover_red.jpg",
		"book_cover_red");

	// the books' printed page edges: navy with pale roses, and red with
	// pink hearts, each a seamless tile
	CreateGLTexture(
		"Textures/book_edge_blue.jpg",
		"book_edge_blue");
	CreateGLTexture(
		"Textures/book_edge_red.jpg",
		"book_edge_red");

	// the ball tin's dark satin metal, with its lid seam. It is 2048x1024
	// because the sphere's texture runs twice as far around as from pole
	// to pole
	CreateGLTexture(
		"Textures/tin_metal.jpg",
		"tin_metal");

	// the tumbler's powder coat
	CreateGLTexture(
		"Textures/tumbler_powdercoat.jpg",
		"tumbler_powdercoat");

	// white plastic for the lamp and the tumbler's lid and handle
	CreateGLTexture(
		"Textures/white_plastic.jpg",
		"white_plastic");

	// the gooseneck's ribbed steel: rings around the tube
	CreateGLTexture(
		"Textures/lamp_gooseneck.jpg",
		"lamp_gooseneck");

	// once the images are in texture memory, bind each one to its own
	// texture unit. The scene uses all 16 slots
	BindGLTextures();
}

/***********************************************************
 *  DefineObjectMaterials()
 *
 *  This method is used for configuring the material of each
 *  surface in the scene. PrepareScene() calls it once, and
 *  the render methods look each material up by its tag with
 *  SetShaderMaterial() before drawing.
 *
 *  Every lit shape but the books' spines and the lamp's
 *  switch is textured, and the texture carries its color.
 *  The ambient and diffuse values only scale how much light
 *  the surface receives, so they stay gray or white. The
 *  materials differ mainly in their highlights, which is
 *  what tells glaze, plastic, and glass apart.
 ***********************************************************/
void SceneManager::DefineObjectMaterials()
{
	// how the project's fragment shader uses each property:
	// - ambientColor times ambientStrength is the share of every light's
	//   ambient color the surface takes, which lights it from all sides
	// - diffuseColor is the share of each light's diffuse light it takes,
	//   strongest where the surface faces the light
	// - both shares multiply the texture's color
	// - specularColor is the share of each light's specular light it
	//   reflects as highlights, which keep the light's color
	// - shininess is the Phong exponent: the higher it is, the smaller and
	//   sharper the highlights

	// black glass for the desktop: it reflects little light diffusely, so
	// most of what shows on it is its highlights and the reflection that
	// shows through it from beneath. Its shininess is the scene's
	// highest, so its highlights are the tightest, as on polished glass
	OBJECT_MATERIAL glassMaterial;
	glassMaterial.ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
	glassMaterial.ambientStrength = 0.5f;
	glassMaterial.diffuseColor = glm::vec3(0.6f, 0.6f, 0.6f);
	glassMaterial.specularColor = glm::vec3(1.0f, 1.0f, 1.0f);
	glassMaterial.shininess = 128.0f;
	glassMaterial.tag = "glass";

	m_objectMaterials.push_back(glassMaterial);

	// glossy ceramic glaze for the mug, inside and out, and its handle
	OBJECT_MATERIAL glazeMaterial;
	glazeMaterial.ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
	glazeMaterial.ambientStrength = 1.0f;
	glazeMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	glazeMaterial.specularColor = glm::vec3(0.6f, 0.6f, 0.6f);
	glazeMaterial.shininess = 48.0f;
	glazeMaterial.tag = "glaze";

	m_objectMaterials.push_back(glazeMaterial);

	// plastic, for the pen, the lamp's switch, and the lamp's and tumbler's
	// white parts: duller than the glaze, with weaker, wider highlights
	OBJECT_MATERIAL plasticMaterial;
	plasticMaterial.ambientColor = glm::vec3(1.0f, 1.0f, 1.0f);
	plasticMaterial.ambientStrength = 1.0f;
	plasticMaterial.diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f);
	plasticMaterial.specularColor = glm::vec3(0.3f, 0.3f, 0.3f);
	plasticMaterial.shininess = 32.0f;
	plasticMaterial.tag = "plastic";

	m_objectMaterials.push_back(plasticMaterial);

	// the remaining surfaces differ in the same way, by their highlights:
	// brushed metal shines broadly, polished steel sharply, satin metal
	// softly, and cloth, paper, and powder coat hardly at all. The
	// tumbler's handle takes 80 percent of the light, since it is a
	// slightly grayer plastic than its white lid. Brushed aluminum also
	// takes 80 percent, since a metal sends much of its light into its sheen
	struct SURFACE
	{
		const char* tag;
		float diffuse;
		float specular;
		float shininess;
	};
	const SURFACE surfaces[] = {
		{ "aluminum", 0.8f, 0.45f, 40.0f },
		{ "steel", 1.0f, 0.8f, 64.0f },
		{ "satin", 1.0f, 0.45f, 30.0f },
		{ "cloth", 1.0f, 0.08f, 8.0f },
		{ "paper", 1.0f, 0.05f, 4.0f },
		{ "powdercoat", 1.0f, 0.15f, 12.0f },
		{ "handle", 0.8f, 0.3f, 20.0f },
	};
	for (const SURFACE& surface : surfaces)
	{
		OBJECT_MATERIAL material;
		material.ambientColor = glm::vec3(surface.diffuse);
		material.ambientStrength = 1.0f;
		material.diffuseColor = glm::vec3(surface.diffuse);
		material.specularColor = glm::vec3(surface.specular);
		material.shininess = surface.shininess;
		material.tag = surface.tag;
		m_objectMaterials.push_back(material);
	}
}

/***********************************************************
 *  SetupSceneLights()
 *
 *  This method is used for adding and configuring the light
 *  sources for the 3D scene and turning the shader's
 *  lighting on. PrepareScene() calls it once. The lights are
 *  kept in m_sceneLights because RenderDeskReflection()
 *  passes them into the shader again, mirrored, every frame.
 *
 *  The four lights stand for the light sources around the
 *  desk in the Milestone One photos: the gooseneck desk lamp,
 *  switched on here, the window on the handle side of the
 *  desk, the window behind the desk, and the gray wall that
 *  faces the desk from the camera's side, which bounces the
 *  room's light back. The windows' daylight lights the scene,
 *  as in the photos, and the lamp adds a warm pool of light
 *  where its shade points.
 ***********************************************************/
void SceneManager::SetupSceneLights()
{
	// colors measured in the photos: the average of the pixels in each
	// region that have no clipped channel, scaled so the brightest channel
	// is 1
	// - light through the sheer curtains on the window beside the desk,
	//   at the left edge of IMG_0060
	const glm::vec3 sideWindowColor = glm::vec3(1.0f, 0.949f, 0.894f);
	// - light through the curtains of the window behind the desk, in the
	//   dining room in IMG_0057
	const glm::vec3 backWindowColor = glm::vec3(1.0f, 0.882f, 0.704f);
	// - the gray wall that faces the desk, behind the camera's starting
	//   place, in IMG_0060
	const glm::vec3 wallColor = glm::vec3(1.0f, 0.925f, 0.834f);

	m_sceneLights.clear();

	// light 0: the desk lamp, a spotlight at the bulb inside the modeled
	// shade, aimed along the shade's axis. The lamp's own placement carries
	// both into the scene, so the light stays in the shade wherever the lamp
	// is placed. The lamp is off in the photos, so it is switched on here
	glm::mat4 lampPlacement = PlacementMatrix(kLampPosition, kLampYawDegrees);
	LIGHT_SOURCE lamp = {};
	lamp.type = LIGHT_SPOT;
	lamp.position = glm::vec3(lampPlacement * glm::vec4(kLampBulb, 1.0f));
	lamp.direction = glm::normalize(glm::vec3(lampPlacement * glm::vec4(kShadeAxis, 0.0f)));
	lamp.ambientColor = 0.02f * kLampColor;
	lamp.diffuseColor = 19.0f * kLampColor;
	lamp.specularColor = kLampColor;
	lamp.specularIntensity = 1.5f;
	// the beam's soft edge. The rim of the mouth stops the bulb's light 41.5
	// degrees from the axis, a cone that spreads it over the middle of the
	// desk with no visible edge. The beam is narrowed inside that, a choice,
	// so it lands as a pool with a visible edge: full strength within the
	// inner cone, fading to nothing at the outer cone
	lamp.innerConeDegrees = 12.0f;
	lamp.outerConeDegrees = 28.0f;
	// LearnOpenGL's attenuation values for a light that reaches 32 units,
	// the shortest reach in its table that covers the whole desk from the
	// bulb
	lamp.constant = 1.0f;
	lamp.linear = 0.14f;
	lamp.quadratic = 0.07f;
	m_sceneLights.push_back(lamp);

	// light 1: daylight from the window beside the desk, a directional
	// light. The window is on the handle (+X) side, toward the camera's
	// end of the desk, so its light travels left, back, and down
	LIGHT_SOURCE sideWindow = {};
	sideWindow.type = LIGHT_DIRECTIONAL;
	sideWindow.direction = glm::normalize(glm::vec3(-1.0f, -0.55f, -0.35f));
	sideWindow.ambientColor = 0.13f * sideWindowColor;
	sideWindow.diffuseColor = 0.34f * sideWindowColor;
	sideWindow.specularColor = sideWindowColor;
	sideWindow.specularIntensity = 0.4f;
	sideWindow.constant = 1.0f;
	m_sceneLights.push_back(sideWindow);

	// light 2: daylight from the window behind the desk, a directional
	// light. In IMG_0057 that window is right of center, so its light
	// comes toward the camera from behind and to the right, and lights the
	// backs and right sides of the objects
	LIGHT_SOURCE backWindow = {};
	backWindow.type = LIGHT_DIRECTIONAL;
	backWindow.direction = glm::normalize(glm::vec3(-0.59f, -0.49f, 0.69f));
	backWindow.ambientColor = 0.11f * backWindowColor;
	backWindow.diffuseColor = 0.23f * backWindowColor;
	backWindow.specularColor = backWindowColor;
	backWindow.specularIntensity = 0.35f;
	backWindow.constant = 1.0f;
	m_sceneLights.push_back(backWindow);

	// light 3: room light bounced off the gray wall, a dim point light in
	// front of the desk, on its center line, 1 unit above and 8 units behind
	// the starting camera (which stands where IMG_0055 was taken). It lights
	// the sides of the objects the camera sees. A matte wall makes no sharp
	// highlight, so it has no specular light, and its constant attenuation
	// keeps it even across the scene
	const glm::vec3 wallBouncePosition = glm::vec3(0.0f, 16.6f, 34.2f);
	LIGHT_SOURCE wallBounce = {};
	wallBounce.type = LIGHT_POINT;
	wallBounce.position = wallBouncePosition;
	wallBounce.ambientColor = 0.10f * wallColor;
	wallBounce.diffuseColor = 0.25f * wallColor;
	wallBounce.specularColor = glm::vec3(0.0f, 0.0f, 0.0f);
	wallBounce.constant = 1.0f;
	m_sceneLights.push_back(wallBounce);

	// pass the lights into the shader's light slots
	ApplySceneLights(false);

	// turn the shader's lighting on. SetShaderLighting() turns it off
	// only around the parts that glow and back on after each, so without
	// this line only the shapes drawn before the first of those, in the
	// first frame's reflection pass, would be drawn unshaded
	m_pShaderManager->setBoolValue(g_UseLightingName, true);
}

/***********************************************************
 *  PrepareScene()
 *
 *  This method is used for preparing the 3D scene by setting
 *  up the materials and lights and loading the textures and
 *  the meshes for the shapes it draws into memory.
 ***********************************************************/
void SceneManager::PrepareScene()
{
	// define the materials the shapes are drawn with. The render methods
	// look each one up by its tag before drawing a shape
	DefineObjectMaterials();
	// add and define the light sources, and turn the shader's lighting on
	SetupSceneLights();

	// load the textures for the 3D scene
	LoadSceneTextures();

	// only one instance of a particular mesh needs to be
	// loaded in memory no matter how many times it is drawn
	// in the rendered 3D scene

	// glass desktop
	m_basicMeshes->LoadPlaneMesh();
	// mug body and interior, pen barrel, and the tumbler's and lamp's cylinders
	m_basicMeshes->LoadCylinderMesh();
	// pen tip
	m_basicMeshes->LoadConeMesh();
	// mug handle: a 180 degree bent tube whose thickness is fixed when it
	// is built, sized here to match the handle band's dark core
	m_mugHandleMesh.Load(180.0f, kHandleTubeFraction, kHandleArcSegments, kHandleTubeSegments);

	// laptop, books, the tumbler's handle, and the lamp's switch
	m_basicMeshes->LoadBoxMesh();
	// ball tin
	m_basicMeshes->LoadSphereMesh();
	// the tumbler's taper and the lamp's shade
	m_basicMeshes->LoadTaperedCylinderMesh();
	// the lamp's gooseneck: 10 segments along its arc and 8 around, 160
	// triangles, with its tube radius as a fraction of its bend radius
	m_gooseneckMesh.Load(kGooseneckSweepDegrees, kLampTubeRadius / kGooseneckBendRadius, 10, 8);
}

/***********************************************************
 *  RenderScene()
 *
 *  This method is used for rendering the 3D scene by
 *  transforming and drawing the basic 3D shapes.
 ***********************************************************/
void SceneManager::RenderScene()
{
	// the glass desktop shows the desk's objects mirrored in it, so the
	// scene is drawn in three steps whose order matters: the mirror image
	// beneath the glass, then the glass blended over it, then the objects
	// themselves
	RenderDeskReflection();
	RenderDeskSurface();
	RenderDeskObjects();
}

/***********************************************************
 *  RenderDeskObjects()
 *
 *  This method is used for drawing every object that stands
 *  on the desk. RenderScene() calls it once for the real
 *  objects, and RenderDeskReflection() calls it once for
 *  their mirror image. Each object but the ball tin, which
 *  is placed directly in scene coordinates, sets its own
 *  placement and clears it when it is done.
 ***********************************************************/
void SceneManager::RenderDeskObjects()
{
	RenderLaptop();
	RenderBookStack();
	RenderBallTin();
	RenderTumbler();
	RenderMugAndPen();
	RenderDeskLamp();
}

/***********************************************************
 *  RenderLaptop()
 *
 *  This method is used for drawing the closed laptop as
 *  three boxes: the lid, the upper base under it, and the
 *  base's lower part, set in from the edges where the real
 *  base curves under. A small unlit box is its green power
 *  light, at the front right as in IMG_0055.
 ***********************************************************/
void SceneManager::RenderLaptop()
{
	SetObjectPlacement(kLaptopPosition, kLaptopYawDegrees);

	// brushed aluminum, its streaks running along the laptop's width; the
	// fallback color is the image's average
	SetShaderColor(0.66f, 0.69f, 0.72f, 1.0f);
	SetShaderTexture("laptop_aluminum");
	SetShaderMaterial("aluminum");

	float lidBottom = kLaptopHeight - kLaptopLidThickness;
	float upperBottom = lidBottom - kLaptopUpperBase;
	glm::vec3 lid(kLaptopWidth, kLaptopLidThickness, kLaptopDepth);
	glm::vec3 upper(kLaptopWidth, kLaptopUpperBase, kLaptopDepth);
	glm::vec3 lower(kLaptopWidth - 2.0f * kLaptopUndersideInset, upperBottom,
		kLaptopDepth - 2.0f * kLaptopUndersideInset);

	DrawBox(lid, glm::vec3(0.0f, lidBottom + 0.5f * lid.y, 0.0f), lid / kAluminumTile);
	DrawBox(upper, glm::vec3(0.0f, upperBottom + 0.5f * upper.y, 0.0f), upper / kAluminumTile);
	DrawBox(lower, glm::vec3(0.0f, 0.5f * lower.y, 0.0f), lower / kAluminumTile);

	// the power light: in IMG_0055 its middle is 26 px (0.47 units) in from
	// the right corner and 6 px (0.13 units) below the seam
	SetShaderLighting(false);
	SetShaderColor(0.35f, 1.0f, 0.45f, 1.0f);
	SetTransformations(glm::vec3(0.14f, 0.05f, 0.02f), 0.0f, 0.0f, 0.0f,
		glm::vec3(0.5f * kLaptopWidth - 0.47f, lidBottom - 0.13f, 0.5f * kLaptopDepth));
	m_basicMeshes->DrawBoxMesh();
	SetShaderLighting(true);

	ClearObjectPlacement();
}

/***********************************************************
 *  RenderBookStack()
 *
 *  This method is used for drawing the two hardcover novels,
 *  one on the other, with RenderBook(): the red one at the
 *  bottom and the blue one on top. In IMG_0055 their front
 *  faces are 67 px tall each, so they are equally thick.
 ***********************************************************/
void SceneManager::RenderBookStack()
{
	BOOK_SPEC book;
	book.position = kBookStackPosition;
	book.YrotationDegrees = kBookYawDegrees;
	book.width = kBookWidth;
	book.length = kBookLength;
	book.thickness = kBookThickness;

	// the bottom book: red cloth and red page edges with pink hearts
	book.coverTexture = "book_cover_red";
	book.edgeTexture = "book_edge_red";
	book.coverColor = glm::vec3(0.41f, 0.16f, 0.19f);
	RenderBook(book);

	// the top book: light blue cloth with roses, and navy page edges
	book.position.y += kBookThickness;
	book.coverTexture = "book_cover_blue";
	book.edgeTexture = "book_edge_blue";
	book.coverColor = glm::vec3(0.58f, 0.61f, 0.74f);
	RenderBook(book);
}

/***********************************************************
 *  RenderBook()
 *
 *  This method is used for drawing one hardcover book from
 *  four boxes: the two cover boards, the spine joining them
 *  along the left long side, and the page block between
 *  them, set in from the boards' edges at the head, tail,
 *  and fore-edge. A book's place, size, textures, and color
 *  come from the BOOK_SPEC, so the same method draws any
 *  hardcover book anywhere on the desk.
 ***********************************************************/
void SceneManager::RenderBook(
	const BOOK_SPEC& book)
{
	SetObjectPlacement(book.position, book.YrotationDegrees);

	float w = book.width;
	float L = book.length;
	float t = book.thickness;

	// the cover image is shaped like the cover, so it maps once across
	// each board's top and bottom. It keeps the same texel size on the
	// board's thin head and tail edges; on its long edges, 0.08 units
	// thick, each texel is 1.5 times as long as it is tall
	SetShaderColor(book.coverColor.r, book.coverColor.g, book.coverColor.b, 1.0f);
	SetShaderTexture(book.coverTexture);
	SetShaderMaterial("cloth");
	glm::vec3 board(w, kBookBoard, L);
	glm::vec3 boardRepeats(1.0f, kBookBoard / L, 1.0f);
	DrawBox(board, glm::vec3(0.0f, 0.5f * kBookBoard, 0.0f), boardRepeats);
	DrawBox(board, glm::vec3(0.0f, t - 0.5f * kBookBoard, 0.0f), boardRepeats);

	// the spine is plain cloth in the cover's color; the cover image would
	// stretch along it, and the real spine is the cover's cloth
	SetShaderColor(book.coverColor.r, book.coverColor.g, book.coverColor.b, 1.0f);
	SetTransformations(glm::vec3(kBookBoard, t - 2.0f * kBookBoard, L), 0.0f, 0.0f, 0.0f,
		glm::vec3(-0.5f * w + 0.5f * kBookBoard, 0.5f * t, 0.0f));
	SetShaderNormals(NORMALS_FROM_MESH, false);
	m_basicMeshes->DrawBoxMesh();

	// the page block runs from the spine to just short of the fore-edge,
	// and is set in at the head and tail, so the boards overhang it
	SetShaderTexture(book.edgeTexture);
	SetShaderMaterial("paper");
	glm::vec3 pages(w - kBookBoard - kBookOverhang, t - 2.0f * kBookBoard, L - 2.0f * kBookOverhang);
	DrawBox(pages, glm::vec3(0.5f * (kBookBoard - kBookOverhang), 0.5f * t, 0.0f), pages / kBookEdgeTile);

	ClearObjectPlacement();
}

/***********************************************************
 *  RenderBallTin()
 *
 *  This method is used for drawing the ball tin on top of
 *  the books as a sphere, tipped so its lid seam lies where
 *  IMG_0055 shows it.
 ***********************************************************/
void SceneManager::RenderBallTin()
{
	SetShaderColor(0.15f, 0.17f, 0.24f, 1.0f);
	SetShaderTexture("tin_metal");
	SetShaderMaterial("satin");
	SetShaderNormals(NORMALS_FROM_MESH, false);
	// the sphere's texture coordinates run once around it and once from
	// pole to pole, the shape of the 2:1 tin image
	// (the shader takes them from the position by longitude and latitude,
	// so the image keeps square corners; see SetSurfaceMapping())
	SetTextureUVScale(1.0f, 1.0f);

	SetTransformations(glm::vec3(kTinRadius), kTinTipXDegrees, 0.0f, kTinTipZDegrees,
		kTinCenter);
	SetSurfaceMapping(MAPPING_SPHERE);
	m_basicMeshes->DrawSphereMesh();
	SetSurfaceMapping(MAPPING_FROM_MESH);
}

/***********************************************************
 *  RenderTumbler()
 *
 *  This method is used for drawing the insulated tumbler as
 *  stacked round parts: a narrow base, a taper up to the
 *  body, the body, a steel band, and the lid, with its fold-
 *  up handle (RenderTumblerHandle()).
 ***********************************************************/
void SceneManager::RenderTumbler()
{
	SetObjectPlacement(kTumblerPosition, kTumblerYawDegrees);

	// powder-coated steel; the image repeats 3 times around each part,
	// and as many times up it as keeps each repeat square
	SetShaderColor(0.80f, 0.76f, 0.71f, 1.0f);
	SetShaderTexture("tumbler_powdercoat");
	SetShaderMaterial("powdercoat");

	// the base, a plain cylinder up to where the taper starts
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);
	SetTransformations(glm::vec3(kTumblerBaseRadius, kTumblerTaperBottom, kTumblerBaseRadius),
		0.0f, 0.0f, 0.0f, glm::vec3(0.0f));
	SetCylinderUVScale(kTumblerBaseRadius, kTumblerTaperBottom, 3.0f);
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	// the taper widens from the base to the body (a ratio of 0.83), while
	// the course tapered cylinder always halves its radius. Turned upside
	// down with its wide end at the body's radius, it narrows to the
	// base's radius 35 percent of the way down, and its narrower rest
	// stays hidden inside the base
	float taperFraction = (kTumblerBodyRadius - kTumblerBaseRadius) / (0.5f * kTumblerBodyRadius);
	float taperLength = (kTumblerTaperTop - kTumblerTaperBottom) / taperFraction;
	SetShaderNormals(NORMALS_TAPERED_SIDE, false);
	SetSegmentTransformations(glm::vec3(0.0f, kTumblerTaperTop, 0.0f),
		glm::vec3(0.0f, kTumblerTaperTop - taperLength, 0.0f), kTumblerBodyRadius);
	SetCylinderUVScale(kTumblerBodyRadius, taperLength, 3.0f);
	SetSurfaceMapping(MAPPING_TAPERED_SIDE, kTumblerBodyRadius, taperLength);
	m_basicMeshes->DrawTaperedCylinderMesh(false, false, true);
	SetSurfaceMapping(MAPPING_FROM_MESH);

	// the body
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);
	float bodyHeight = kTumblerBandBottom - kTumblerTaperTop;
	SetTransformations(glm::vec3(kTumblerBodyRadius, bodyHeight, kTumblerBodyRadius),
		0.0f, 0.0f, 0.0f, glm::vec3(0.0f, kTumblerTaperTop, 0.0f));
	SetCylinderUVScale(kTumblerBodyRadius, bodyHeight, 3.0f);
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	// the stainless band under the lid, in brushed metal
	SetShaderColor(0.66f, 0.69f, 0.72f, 1.0f);
	SetShaderTexture("laptop_aluminum");
	SetShaderMaterial("steel");
	float bandHeight = kTumblerBandTop - kTumblerBandBottom;
	SetTransformations(glm::vec3(kTumblerLidRadius, bandHeight, kTumblerLidRadius),
		0.0f, 0.0f, 0.0f, glm::vec3(0.0f, kTumblerBandBottom, 0.0f));
	SetCylinderUVScale(kTumblerLidRadius, bandHeight, 4.0f);
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	// the lid, white plastic, with its flat top
	SetShaderColor(0.89f, 0.90f, 0.89f, 1.0f);
	SetShaderTexture("white_plastic");
	SetShaderMaterial("plastic");
	float lidHeight = kTumblerLidTop - kTumblerBandTop;
	SetTransformations(glm::vec3(kTumblerLidRadius, lidHeight, kTumblerLidRadius),
		0.0f, 0.0f, 0.0f, glm::vec3(0.0f, kTumblerBandTop, 0.0f));
	SetCylinderUVScale(kTumblerLidRadius, lidHeight, 3.0f);
	m_basicMeshes->DrawCylinderMesh(false, false, true);
	SetShaderNormals(NORMALS_FROM_MESH, false);
	SetTextureUVScale(1.0f, 1.0f);
	SetSurfaceMapping(MAPPING_CYLINDER_CAP);
	m_basicMeshes->DrawCylinderMesh(true, false, false);
	SetSurfaceMapping(MAPPING_FROM_MESH);

	RenderTumblerHandle();

	ClearObjectPlacement();
}

/***********************************************************
 *  RenderTumblerHandle()
 *
 *  This method is used for drawing the tumbler's fold-up
 *  handle as a loop of five straight straps, in the plane
 *  through the tumbler's axis (its local x and y), and the
 *  two round pivots it turns on, flat discs on the lid's
 *  sides. RenderTumbler() calls it with the tumbler's
 *  placement already set.
 ***********************************************************/
void SceneManager::RenderTumblerHandle()
{
	// light gray plastic: the lid's white image under a material that
	// takes less of the light
	SetShaderColor(0.66f, 0.68f, 0.72f, 1.0f);
	SetShaderTexture("white_plastic");
	SetShaderMaterial("handle");

	// the loop's corners, from the left pivot over the top to the right
	const glm::vec2 path[] = {
		glm::vec2(-kHandlePivotOut, kHandlePivotHeight),
		glm::vec2(-kHandlePivotOut, kHandleSideTop),
		glm::vec2(-kHandleBarHalfLength, kHandleBarHeight),
		glm::vec2(kHandleBarHalfLength, kHandleBarHeight),
		glm::vec2(kHandlePivotOut, kHandleSideTop),
		glm::vec2(kHandlePivotOut, kHandlePivotHeight),
	};
	const int kCorners = sizeof(path) / sizeof(path[0]);

	// each strap is a box stretched from one corner to the next and turned
	// about z to lie along that stretch; it runs half its thickness past
	// each corner so neighboring straps overlap at the bends
	for (int i = 0; i + 1 < kCorners; i++)
	{
		glm::vec2 a = path[i];
		glm::vec2 b = path[i + 1];
		glm::vec2 middle = 0.5f * (a + b);
		float length = glm::length(b - a) + kHandleStrapThickness;
		float angle = glm::degrees(glm::atan(b.y - a.y, b.x - a.x));
		glm::vec3 size(length, kHandleStrapThickness, kHandleStrapWidth);

		SetTransformations(size, 0.0f, 0.0f, angle, glm::vec3(middle, 0.0f));
		SetShaderNormals(NORMALS_FROM_MESH, false);
		SetBoxProjection(size / 2.0f);
		m_basicMeshes->DrawBoxMesh();
		ClearBoxProjection();
	}

	// the pivots: the cylinder's top cap, turned to face out of each side
	// of the lid
	SetTextureUVScale(1.0f, 1.0f);
	SetSurfaceMapping(MAPPING_CYLINDER_CAP);
	for (int side = -1; side <= 1; side += 2)
	{
		SetTransformations(glm::vec3(0.32f, 0.04f, 0.32f), 0.0f, 0.0f, -90.0f * side,
			glm::vec3(side * (kTumblerLidRadius + 0.01f), kHandlePivotHeight, 0.0f));
		m_basicMeshes->DrawCylinderMesh(true, false, false);
	}
	SetSurfaceMapping(MAPPING_FROM_MESH);
}

/***********************************************************
 *  RenderDeskLamp()
 *
 *  This method is used for drawing the gooseneck desk lamp:
 *  its round base, the smooth stem, the ribbed gooseneck (a
 *  bent tube), the shade's vented neck, and its bell, with
 *  the mouth drawn as a glowing disc because the lamp is
 *  switched on. The spotlight in SetupSceneLights() shines
 *  from inside this bell.
 ***********************************************************/
void SceneManager::RenderDeskLamp()
{
	SetObjectPlacement(kLampPosition, kLampYawDegrees);

	// the base, white plastic, with its flat top
	SetShaderColor(0.89f, 0.90f, 0.89f, 1.0f);
	SetShaderTexture("white_plastic");
	SetShaderMaterial("plastic");
	SetTransformations(glm::vec3(kLampBaseRadius, kLampBaseHeight, kLampBaseRadius),
		0.0f, 0.0f, 0.0f, glm::vec3(0.0f));
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);
	SetCylinderUVScale(kLampBaseRadius, kLampBaseHeight, 4.0f);
	m_basicMeshes->DrawCylinderMesh(false, false, true);
	SetShaderNormals(NORMALS_FROM_MESH, false);
	SetTextureUVScale(1.0f, 1.0f);
	SetSurfaceMapping(MAPPING_CYLINDER_CAP);
	m_basicMeshes->DrawCylinderMesh(true, false, false);
	SetSurfaceMapping(MAPPING_FROM_MESH);

	// the rocker switch on the base's top, black plastic (IMG_0055 shows it
	// at the base's front right)
	SetShaderColor(0.04f, 0.04f, 0.05f, 1.0f);
	SetShaderMaterial("plastic");
	SetTransformations(glm::vec3(0.45f, 0.18f, 0.32f), 0.0f, 0.0f, 0.0f,
		glm::vec3(1.2f, kLampBaseHeight + 0.09f, 1.3f));
	m_basicMeshes->DrawBoxMesh();

	// the smooth stem, polished steel
	SetShaderColor(0.59f, 0.58f, 0.57f, 1.0f);
	SetShaderTexture("laptop_aluminum");
	SetShaderMaterial("steel");
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);
	SetSegmentTransformations(kLampStemFoot, kLampStemTop, kLampTubeRadius);
	SetCylinderUVScale(kLampTubeRadius, glm::length(kLampStemTop - kLampStemFoot), 1.0f);
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	// the gooseneck. The bent tube's ring has radius 1 and starts on its
	// +X axis heading toward +Y, so +X is turned to point from the bend's
	// center back to the stem's top, +Y straight up, and the ring scaled
	// to the bend radius
	glm::vec3 bendCenter = kLampStemTop + kGooseneckBendDirection * kGooseneckBendRadius;
	glm::vec3 ringX = -kGooseneckBendDirection;
	glm::vec3 ringY = glm::vec3(0.0f, 1.0f, 0.0f);
	glm::mat4 ring(
		glm::vec4(ringX, 0.0f),
		glm::vec4(ringY, 0.0f),
		glm::vec4(glm::cross(ringX, ringY), 0.0f),
		glm::vec4(bendCenter, 1.0f));
	SetModelMatrix(ring * glm::scale(glm::vec3(kGooseneckBendRadius)));
	SetShaderTexture("lamp_gooseneck");
	SetShaderNormals(NORMALS_FROM_MESH, false);
	// u runs once along the arc and v once around the tube
	float gooseneckLength = kGooseneckBendRadius * glm::radians(kGooseneckSweepDegrees);
	SetTextureUVScale(gooseneckLength / kGooseneckRibTile, 1.0f);
	m_gooseneckMesh.Draw();

	// the shade's vented neck: a cylinder along the shade's axis from its
	// back end, closed there by its end cap
	SetShaderColor(0.89f, 0.90f, 0.89f, 1.0f);
	SetShaderTexture("white_plastic");
	SetShaderMaterial("plastic");
	SetSegmentTransformations(kShadeEndCap, kShadeEndCap + kShadeAxis * kShadeHousingLength, kShadeHousingRadius);
	SetCylinderUVScale(kShadeHousingRadius, kShadeHousingLength, 3.0f);
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);
	m_basicMeshes->DrawCylinderMesh(false, false, true);
	SetShaderNormals(NORMALS_FROM_MESH, false);
	SetTextureUVScale(1.0f, 1.0f);
	SetSurfaceMapping(MAPPING_CYLINDER_CAP);
	m_basicMeshes->DrawCylinderMesh(false, true, false);
	SetSurfaceMapping(MAPPING_FROM_MESH);

	// the bell: the course tapered cylinder, wide end at the mouth. The
	// mouth is 4.64 units across in IMG_0055, and the tapered cylinder
	// halves that toward the neck
	SetSegmentTransformations(kShadeMouthCenter, kLampBulb, kShadeMouthRadius);
	SetCylinderUVScale(kShadeMouthRadius, kShadeBellLength, 3.0f);
	SetShaderNormals(NORMALS_TAPERED_SIDE, false);
	SetSurfaceMapping(MAPPING_TAPERED_SIDE, kShadeMouthRadius, kShadeBellLength);
	m_basicMeshes->DrawTaperedCylinderMesh(false, false, true);
	SetSurfaceMapping(MAPPING_FROM_MESH);

	// the lit mouth: a disc just inside the rim, drawn unlit in the bulb's
	// color and brighter toward its middle. The cylinder's bottom cap faces
	// back along its axis, so the segment runs into the shade and the cap
	// faces out of the mouth
	glm::vec3 discCenter = kShadeMouthCenter - kShadeAxis * 0.15f;
	SetShaderLighting(false);
	SetShaderColor(kLampColor.r, kLampColor.g, kLampColor.b, 1.0f);
	m_pShaderManager->setFloatValue(g_GlowCoreGainName, kLampGlowCoreGain);
	SetSegmentTransformations(discCenter, discCenter - kShadeAxis * 0.01f, 0.96f * kShadeMouthRadius);
	m_basicMeshes->DrawCylinderMesh(false, true, false);
	m_pShaderManager->setFloatValue(g_GlowCoreGainName, 1.0f);
	SetShaderLighting(true);

	ClearObjectPlacement();
}

/***********************************************************
 *  RenderMugAndPen()
 *
 *  This method is used for drawing every part of the mug and
 *  the pen, as one object standing at kMugPosition.
 ***********************************************************/
void SceneManager::RenderMugAndPen()
{
	SetObjectPlacement(kMugPosition, 0.0f);

	// each part has its own method so its shape, transformations,
	// material, and texture can be read in one place, and depth testing
	// decides which part shows in front
	RenderMugBody();
	RenderMugInterior();
	RenderMugHandle();
	RenderPen();

	ClearObjectPlacement();
}

/***********************************************************
 *  RenderDeskReflection()
 *
 *  This method is used for drawing the desk's objects
 *  mirrored beneath the glass desktop, where they show
 *  through the glass the way they do in IMG_0055 and
 *  IMG_0057. The stencil buffer keeps the mirror image to
 *  the pixels where the glass itself is visible, so it
 *  never shows past the desk's edges.
 ***********************************************************/
void SceneManager::RenderDeskReflection()
{
	// step 1: mark the visible glass. The desktop is drawn into the
	// stencil buffer's glass bit only, with color and depth writes off, so
	// the screen and the depth buffer are left as they were
	glEnable(GL_STENCIL_TEST);
	glStencilMask(0xFF);
	glClear(GL_STENCIL_BUFFER_BIT);
	glStencilFunc(GL_ALWAYS, kGlassStencilBit, kGlassStencilBit);
	glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	DrawDeskPlane();

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthMask(GL_TRUE);

	// step 2: draw the mirror image only where the glass bit is set, and
	// leave the stencil buffer as it is. SetModelMatrix() flips every
	// shape below the desktop while m_bDrawingReflection is true, and the
	// lights move to their mirror images too, so the reflection is lit
	// from the same side as the real objects
	glStencilFunc(GL_EQUAL, kGlassStencilBit, kGlassStencilBit);
	glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
	glStencilMask(0x00);

	m_bDrawingReflection = true;
	ApplySceneLights(true);

	RenderDeskObjects();

	// step 3: put the lights and the drawing state back for the real scene
	m_bDrawingReflection = false;
	ApplySceneLights(false);

	glStencilMask(0xFF);
	glDisable(GL_STENCIL_TEST);
}

/***********************************************************
 *  DrawDeskPlane()
 *
 *  This method is used for setting the desktop's
 *  transformations and drawing its plane. Both the stencil
 *  pass in RenderDeskReflection() and RenderDeskSurface() use
 *  it, so they cover exactly the same pixels.
 ***********************************************************/
void SceneManager::DrawDeskPlane()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// the plane mesh spans -1 to 1 in X and Z, so half the desk's width
	// and depth scale it to the measured desktop
	scaleXYZ = glm::vec3(0.5f * kDeskWidth, 1.0f, 0.5f * kDeskDepth);

	// the plane mesh already lies flat, facing up
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// the desktop is the ground level, y = 0, for the whole scene
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

	// set the transformations into the shader for the next mesh drawn
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// the plane's stored normal, straight up, is already exact
	SetShaderNormals(NORMALS_FROM_MESH, false);

	// draw the mesh with transformation values
	m_basicMeshes->DrawPlaneMesh();
}

/***********************************************************
 *  PlacementMatrix()
 *
 *  This method is used for building the matrix that stands
 *  an object at a position on the desk, turned about the
 *  vertical axis. It turns the object first, about its own
 *  origin, then moves it, so the object's origin lands on
 *  the position.
 ***********************************************************/
glm::mat4 SceneManager::PlacementMatrix(
	glm::vec3 position,
	float YrotationDegrees)
{
	return(glm::translate(position) *
		glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f)));
}

/***********************************************************
 *  SetObjectPlacement()
 *
 *  This method is used for placing the parts drawn next as
 *  one object. Each part's own transformations place it
 *  within the object, and SetModelMatrix() applies this
 *  placement after them, so a function that draws an object
 *  can draw it anywhere on the desk, facing any way, and
 *  more than once.
 ***********************************************************/
void SceneManager::SetObjectPlacement(
	glm::vec3 position,
	float YrotationDegrees)
{
	m_objectPlacement = PlacementMatrix(position, YrotationDegrees);
}

/***********************************************************
 *  ClearObjectPlacement()
 *
 *  This method is used for going back to placing shapes
 *  directly in scene coordinates.
 ***********************************************************/
void SceneManager::ClearObjectPlacement()
{
	m_objectPlacement = glm::mat4(1.0f);
}

/***********************************************************
 *  RotateOffset()
 *
 *  This method is used for rotating a local offset exactly
 *  as SetTransformations() rotates a mesh. Both multiply the
 *  X, Y, and Z rotation matrices in X * Y * Z order, so the
 *  offset is turned about Z first, then Y, then X. A part
 *  attached to the end of a rotated part, such as the pen tip
 *  on the pen barrel, uses it to find where that end landed,
 *  so the two stay joined at any angle.
 ***********************************************************/
glm::vec3 SceneManager::RotateOffset(
	glm::vec3 offset,
	float XrotationDegrees,
	float YrotationDegrees,
	float ZrotationDegrees)
{
	glm::mat4 rotation =
		glm::rotate(glm::radians(XrotationDegrees), glm::vec3(1.0f, 0.0f, 0.0f)) *
		glm::rotate(glm::radians(YrotationDegrees), glm::vec3(0.0f, 1.0f, 0.0f)) *
		glm::rotate(glm::radians(ZrotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));

	// a w of 0 marks the offset as a direction, so only the rotation applies
	return(glm::vec3(rotation * glm::vec4(offset, 0.0f)));
}

/***********************************************************
 *  RenderDeskSurface()
 *
 *  This method is used for drawing the glass desktop that
 *  the mug stands on, blended over the reflection that
 *  RenderDeskReflection() has already drawn beneath it.
 ***********************************************************/
void SceneManager::RenderDeskSurface()
{
	// IMG_0056 samples the glass near black, RGB (12, 11, 11), and
	// Milestone Two lifted it to this dark gray so the desk stays visible
	// against the black background. The glass texture averages this color,
	// which stays underneath as the fallback
	SetShaderColor(0.16f, 0.16f, 0.17f, 1.0f);
	SetShaderTexture("desk_glass");

	// the plane's texture coordinates run once across the whole desk, so
	// this many repeats across and deep make each repeat a 10 unit square
	// and keep the texels square. The glass image's opposite edges match,
	// so a repeat may stop part way at the desk's edges
	SetTextureUVScale(kDeskWidth / kDeskTileSize, kDeskDepth / kDeskTileSize);

	SetShaderMaterial("glass");

	// turn on the shader's reflection for this surface alone. Its output
	// is premultiplied by its opacity, so this blending adds it to the
	// reflection already drawn after scaling the reflection down to the
	// share the glass reflects at each pixel's viewing angle
	m_pShaderManager->setFloatValue(g_ReflectanceName, kGlassReflectance);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	DrawDeskPlane();

	glDisable(GL_BLEND);
	m_pShaderManager->setFloatValue(g_ReflectanceName, 0.0f);
}

/***********************************************************
 *  RenderMugBody()
 *
 *  This method is used for drawing the outer wall of the mug
 *  as an open-topped cylinder, textured with the tiled
 *  flecked glaze and the UCONN HEALTH print laid over it,
 *  both combined by the fragment shader in one draw.
 ***********************************************************/
void SceneManager::RenderMugBody()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// the cylinder mesh has radius 1 and runs from y = 0 to y = 1,
	// so X and Z set the radius and Y sets the height. In IMG_0057
	// the body's left edge stays within columns 1442 to 1445 from row
	// 890 to row 1030, and its right edge stays within columns 1646 to
	// 1653 on rows 960, 980, 1040, 1045, and 1050, where no part of the
	// handle touches that edge, so one straight cylinder fits
	scaleXYZ = glm::vec3(kMugRadius, kMugHeight, kMugRadius);

	// the mug stands upright, and a cylinder looks the same at any
	// turn about its own axis, so it needs no rotation
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// the base of the mesh is at its local origin, so a position at
	// y = 0 stands the mug on the desktop
	positionXYZ = glm::vec3(0.0f, 0.0f, 0.0f);

	// set the transformations into the shader for the next mesh drawn
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// navy glaze; the lower body in IMG_0056, including the part of the
	// handle in front of it, averages RGB (8, 7, 23). Milestone Two, drawn
	// before the scene had lighting, kept that blue-heavy balance but
	// brightened it. The glaze texture averages this color, which stays
	// underneath and shows only if the texture failed to load
	SetShaderColor(0.14f, 0.12f, 0.40f, 1.0f);
	SetShaderTexture("mug_glaze");
	SetShaderMaterial("glaze");

	// the side of the course cylinder is shaded with its exact normal,
	// which points straight out from the mug's axis
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);

	// the glaze repeats 4 times around the 12.57 unit circumference and
	// 1.08 times up the 3.4 unit height, so every repeat covers a 3.14
	// unit square and the flecks keep their measured shape
	SetCylinderUVScale(kMugRadius, kMugHeight, kGlazeTilesAround);

	// the UCONN HEALTH print is a second image, laid over the glaze by
	// the fragment shader in the same draw. It is 4096x1108, the same 3.7
	// to 1 shape as the wall unrolled (12.57 by 3.4 units), and it is not
	// repeated, so it wraps onto the wall without being stretched. In
	// IMG_0057, taken with the handle side-on, each letter's left and
	// right edges were converted from pixel columns to angles around the
	// mug, since the curve squeezes letters toward the sides: an angle
	// is the arcsine of a column's distance from the body's center line
	// over the body's 102 px radius. Each UCONN letter spans 17.2 to
	// 17.6 degrees and the line centers 8.8 degrees toward the handle
	// from the middle of the visible face. The last N ends so near the
	// silhouette that one pixel there spans over a degree, so it takes
	// the other letters' width. HEALTH spans 8.0 to 11.2 degrees per
	// letter and centers 8.3 degrees toward the handle. The mesh puts
	// u = 0.75 at +Z, facing the front of the desk, and the handle at
	// u = 0, so an angle a lands at u = 0.75 + a / 360. The caps of
	// UCONN run 1.45 to 2.09 units below the rim and those of HEALTH
	// 2.29 to 2.66 units, at 0.0196 units per px. The letters are DejaVu
	// Sans Bold, a freely licensed font, each scaled to fill its
	// measured box. Rendered in a level orthographic view, HEALTH spans
	// the same share of the mug's width as in IMG_0057 to within 0.4
	// percent of that width, and UCONN 1.1 percent less
	SetShaderOverlay("mug_lettering", 1.0f, 1.0f);

	// draw the sides only: the top stays open like the real mug, and
	// a bottom cap would sit flush with the desk where it is never seen
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	// the shapes drawn after the mug body show their own texture alone
	ClearShaderOverlay();
}

/***********************************************************
 *  RenderMugInterior()
 *
 *  This method is used for drawing the inside wall and floor
 *  of the mug, which gives the open rim a visible thickness.
 ***********************************************************/
void SceneManager::RenderMugInterior()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// the inside is narrower than the body by the wall thickness, and
	// shorter by the thickness of the floor, so both rims end level
	scaleXYZ = glm::vec3(
		kMugRadius - kMugWallThickness,
		kMugHeight - kMugWallThickness,
		kMugRadius - kMugWallThickness);

	// the interior shares the body's upright axis
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = 0.0f;

	// raising the interior by the floor thickness keeps its floor
	// above the desktop instead of fighting it for the same depth
	positionXYZ = glm::vec3(0.0f, kMugWallThickness, 0.0f);

	// set the transformations into the shader for the next mesh drawn
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// the shaded inside samples average RGB (1, 1, 9) in IMG_0056, so
	// it is a much darker navy than the outer wall. The interior glaze
	// texture averages this color, which stays underneath as the fallback
	SetShaderColor(0.02f, 0.02f, 0.18f, 1.0f);
	SetShaderTexture("mug_glaze_interior");
	SetShaderMaterial("glaze");

	// the camera sees the inside face of this cylinder, but the mesh's
	// normals point out of it, so they are turned inward. The wall uses the
	// cylinder side's exact normal, like the outer wall
	SetShaderNormals(NORMALS_CYLINDER_SIDE, true);

	// the inside wall repeats the glaze the same whole number of times
	// around as the outer wall, which keeps its wrap seam hidden too. Its
	// radius is 8 percent smaller, so its flecks are 8 percent smaller
	// than the outer wall's
	SetCylinderUVScale(
		kMugRadius - kMugWallThickness,
		kMugHeight - kMugWallThickness,
		kGlazeTilesAround);

	// draw the sides; the top stays open
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	// the floor's texture coordinates fill the image with a disk as wide
	// as the floor, 2r across. One wall repeat is 2 pi r / 4 wide, so
	// 2r / (2 pi r / 4) = 4 / pi repeats across the floor give it the
	// wall's fleck size at any radius
	float floorTiles = kGlazeTilesAround / glm::pi<float>();
	SetTextureUVScale(floorTiles, floorTiles);

	// the floor is the mesh's bottom cap, whose stored normal points down
	// out of the cylinder; turned inward, it points up into the mug
	SetShaderNormals(NORMALS_FROM_MESH, true);

	// draw the floor
	SetSurfaceMapping(MAPPING_CYLINDER_CAP);
	m_basicMeshes->DrawCylinderMesh(false, true, false);
	SetSurfaceMapping(MAPPING_FROM_MESH);
}

/***********************************************************
 *  RenderMugHandle()
 *
 *  This method is used for drawing the looped handle on the
 *  right side of the mug as a 180 degree bent tube, half of
 *  a torus.
 ***********************************************************/
void SceneManager::RenderMugHandle()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// the bent tube's ring lies in the XY plane with ring radius 1, and
	// its 180 degree arc is the half above its center. Scale is applied
	// before rotation, so local X becomes the handle's height and local Y
	// becomes its reach. Local X is divided by (1 + tube fraction)
	// because the outer edge sits one tube radius past the ring. Local
	// Z is divided by the tube fraction because the tube is the only
	// part of the mesh with any depth, which makes the handle a wide,
	// flat band as it appears from above in IMG_0058
	scaleXYZ = glm::vec3(
		kHandleHalfSpan / (1.0f + kHandleTubeFraction),
		kHandleReach,
		kHandleHalfDepth / kHandleTubeFraction);

	// turning -90 degrees about Z swings the arc from above its center
	// to the right of it, the side the handle is on in IMG_0055 and
	// IMG_0057, and stands both open ends of the arc against the body
	XrotationDegrees = 0.0f;
	YrotationDegrees = 0.0f;
	ZrotationDegrees = -90.0f;

	// the center is set one tube radius inside the body wall, which
	// hides the open ends of the arc inside the wall and puts the
	// outer edge of the loop exactly kHandleReach past the body
	positionXYZ = glm::vec3(
		kMugRadius - kHandleTubeRadius,
		kHandleCenterHeight,
		0.0f);

	// set the transformations into the shader for the next mesh drawn
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// the handle is glazed the same navy as the body, with a version of
	// the body's glaze whose flecks land at the body's size, so the glaze
	// reads as one surface where the handle meets the wall
	SetShaderColor(0.14f, 0.12f, 0.40f, 1.0f);
	SetShaderTexture("mug_glaze_handle");
	SetShaderMaterial("glaze");

	// the bent tube stores each vertex's exact normal, which the normal
	// matrix turns with the -90 degree rotation and the flattening scale
	SetShaderNormals(NORMALS_FROM_MESH, false);

	// along the handle, u runs once from 0 to 1. The length of the arc's
	// center line after the scale above, about 4.59 units, sets how many
	// glaze repeats keep them the body's 3.14 units long. Around the tube,
	// v runs once from 0 to 1 and the image has to meet itself there, so
	// it repeats exactly once. The tube is only 1.70 to 1.77 units around,
	// so the image is squeezed about 1.81 times across the band, which the
	// handle's glaze image makes up for by drawing its flecks 1.81 times
	// taller
	const int kLengthSteps = 64;
	float handleLength = 0.0f;
	for (int i = 0; i < kLengthSteps; i++)
	{
		float angle0 = glm::pi<float>() * i / kLengthSteps;
		float angle1 = glm::pi<float>() * (i + 1) / kLengthSteps;
		glm::vec2 point0(glm::cos(angle0) * scaleXYZ.x, glm::sin(angle0) * scaleXYZ.y);
		glm::vec2 point1(glm::cos(angle1) * scaleXYZ.x, glm::sin(angle1) * scaleXYZ.y);
		handleLength += glm::length(point1 - point0);
	}
	SetTextureUVScale(handleLength / kGlazeTileWidth, 1.0f);

	m_mugHandleMesh.Draw();
}

/***********************************************************
 *  RenderPen()
 *
 *  This method is used for drawing the teal pen leaning in
 *  the mug: a cylinder for the barrel, capped by a cone for
 *  the tip.
 ***********************************************************/
void SceneManager::RenderPen()
{
	// declare the variables for the transformations
	glm::vec3 scaleXYZ;
	float XrotationDegrees = 0.0f;
	float YrotationDegrees = 0.0f;
	float ZrotationDegrees = 0.0f;
	glm::vec3 positionXYZ;

	// the direction the pen points after its lean. The Z rotation
	// tips it toward the left, then the Y rotation swings that lean
	// toward the back of the desk
	glm::vec3 penAxis = RotateOffset(
		glm::vec3(0.0f, 1.0f, 0.0f),
		0.0f,
		kPenYawDegrees,
		kPenTiltDegrees);

	// the estimated point where the pen crosses the rim
	glm::vec3 rimEntry = glm::vec3(kPenEntryX, kMugHeight, kPenEntryZ);

	// the Y rotation does not change height, so penAxis.y is the rise
	// per unit of pen length; dividing the drop from the rim to the
	// mug floor by it gives the length of pen hidden inside the mug
	float lengthInsideMug = (kMugHeight - kMugWallThickness) / penAxis.y;
	glm::vec3 penBase = rimEntry - (penAxis * lengthInsideMug);
	float barrelLength = lengthInsideMug + kPenLengthAboveRim - kPenTipLength;

	/*** PEN BARREL ***/
	// a thin cylinder, stretched along its own Y axis to the barrel length
	scaleXYZ = glm::vec3(kPenRadius, barrelLength, kPenRadius);

	// the measured lean
	XrotationDegrees = 0.0f;
	YrotationDegrees = kPenYawDegrees;
	ZrotationDegrees = kPenTiltDegrees;

	// the base of the barrel rests on the floor of the mug
	positionXYZ = penBase;

	// set the transformations into the shader for the next mesh drawn
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// teal; the barrel samples average RGB (109, 196, 196) in IMG_0056.
	// The barrel texture averages this color, which stays underneath as
	// the fallback
	SetShaderColor(0.43f, 0.77f, 0.77f, 1.0f);
	SetShaderTexture("pen_barrel");
	SetShaderMaterial("plastic");

	// one repeat around the thin barrel, and as many along it as keep
	// each repeat square
	SetCylinderUVScale(kPenRadius, barrelLength, 1.0f);

	// the side takes the cylinder side's exact normal. The end caps are
	// not drawn, which saves 68 triangles: the tip covers the top one, and
	// the bottom one rests on the mug's floor, hidden by the barrel and
	// the mug's wall
	SetShaderNormals(NORMALS_CYLINDER_SIDE, false);
	m_basicMeshes->DrawCylinderMesh(false, false, true);

	/*** PEN TIP ***/
	// a cone the same radius as the barrel, so the two meet flush
	scaleXYZ = glm::vec3(kPenRadius, kPenTipLength, kPenRadius);

	// the tip shares the barrel's lean
	XrotationDegrees = 0.0f;
	YrotationDegrees = kPenYawDegrees;
	ZrotationDegrees = kPenTiltDegrees;

	// the barrel's top end is its base plus its length along the same
	// rotation, so the cone's base sits exactly on the end of the barrel
	positionXYZ = penBase + RotateOffset(
		glm::vec3(0.0f, barrelLength, 0.0f),
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees);

	// set the transformations into the shader for the next mesh drawn
	SetTransformations(
		scaleXYZ,
		XrotationDegrees,
		YrotationDegrees,
		ZrotationDegrees,
		positionXYZ);

	// the tip's teal, measured against the barrel in IMG_0056 (see the
	// texture comment below), which stays underneath as the fallback
	SetShaderColor(0.25f, 0.63f, 0.60f, 1.0f);
	SetShaderTexture("pen_tip");
	SetShaderMaterial("plastic");

	// the course cone stores flat, sideways normals on its side although
	// that side slopes at 45 degrees, so the tip uses the side's exact
	// normal, which the normal matrix then fits to the tip's narrower slope
	SetShaderNormals(NORMALS_CONE_SIDE, false);

	// the tip's texture is laid out as rings: a gray metal point at the
	// center, the teal taper around it, and the thin dark joint at the
	// outer edge. The cone's texture coordinates project its side
	// straight down onto the image, so all 36 apex vertices sit at the
	// image center and the distance from the center grows with the drop
	// from the apex, reaching 0.5 at the base (within 0.011 over all 108
	// side vertices). Each ring's width in the image is therefore its
	// share of the tip's length. In IMG_0056 the metal point is 9 px and
	// the joint about 2 px of the 78 px from the joint to the point. The
	// taper's color there is RGB (84, 190, 183) against the barrel's
	// (141, 233, 236) on the same frame, and those ratios applied to the
	// barrel's average give the teal, RGB (65, 160, 152), used above
	SetTextureUVScale(1.0f, 1.0f);

	// the cone's base is hidden against the barrel, so it is not drawn
	m_basicMeshes->DrawConeMesh(false);
}
