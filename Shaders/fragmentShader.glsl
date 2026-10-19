#version 440 core
///////////////////////////////////////////////////////////////////////////////
// fragmentShader.glsl
// ============
// color each fragment, usually with the Phong lighting model: an ambient, a
// diffuse, and a specular term from each of up to four light sources
//
// This is the 7-1 project's own copy of the course fragment shader in
// Utilities/shaders, which other projects still use unchanged. Compared
// with that shader, this one
// - reads each light's diffuse and specular colors, so a colored light
//   shades surfaces instead of only tinting them evenly
// - supports directional lights, point lights, and spotlights, and adds
//   nothing for a light slot that is switched off
// - combines two textures in one draw: an overlay image, such as the mug's
//   lettering, is laid over the base texture by its own transparency
// - can map a texture onto each face of a box at a fixed size, so box
//   faces of any shape show it unstretched (see SurfaceCoordinate())
// - rebuilds exact normals for the course cylinder, cone, and tapered
//   cylinder meshes, whose stored normals are off (see SurfaceNormal())
// - lets the glass desktop reflect more strongly toward grazing angles
// - brightens a glowing part toward its middle, for the lamp's lit mouth
// - makes texture coordinates for the course cylinder's caps, tapered
//   cylinder's side, and sphere from the surface position, so the caps
//   and the tapered side are not stretched or skewed and the sphere's
//   texels keep square corners (see SurfaceCoordinate() and SampleTexture())
// - leaves out mirrored fragments that touch the glass, which would show
//   through it as specks (see main())
///////////////////////////////////////////////////////////////////////////////

struct Material
{
    vec3 ambientColor;
    float ambientStrength;
    vec3 diffuseColor;
    vec3 specularColor;
    float shininess;
};

struct LightSource
{
    int type;
    vec3 position;
    vec3 direction;
    vec3 ambientColor;
    vec3 diffuseColor;
    vec3 specularColor;
    float specularIntensity;
    float innerConeCos;
    float outerConeCos;
    float constant;
    float linear;
    float quadratic;
};

#define TOTAL_LIGHTS 4

// light types, matching SceneManager::LightType
const int LIGHT_OFF = 0;
const int LIGHT_DIRECTIONAL = 1;
const int LIGHT_POINT = 2;
const int LIGHT_SPOT = 3;

// where a surface's normal comes from, matching SceneManager::NormalSource
const int NORMALS_FROM_MESH = 0;
const int NORMALS_CYLINDER_SIDE = 1;
const int NORMALS_CONE_SIDE = 2;
const int NORMALS_TAPERED_SIDE = 3;

// how a surface's texture coordinates are made, matching
// SceneManager::SurfaceMapping
const int MAPPING_FROM_MESH = 0;
const int MAPPING_CYLINDER_CAP = 1;
const int MAPPING_TAPERED_SIDE = 2;
const int MAPPING_SPHERE = 3;

in vec3 fragmentPosition;
in vec3 fragmentVertexNormal;
in vec3 fragmentMeshPosition;
in vec2 fragmentTextureCoordinate;

out vec4 outFragmentColor;

uniform bool bUseTexture = false;
uniform bool bUseLighting = false;
uniform vec4 objectColor = vec4(1.0f);
uniform sampler2D objectTexture;
uniform vec3 viewPosition;
uniform vec2 UVscale = vec2(1.0f, 1.0f);
// a second image laid over the base texture, with its own UV scale
uniform bool bUseOverlay = false;
uniform sampler2D overlayTexture;
uniform vec2 overlayUVscale = vec2(1.0f, 1.0f);
// for the course box: take texture coordinates from each face's position on
// the unscaled mesh, repeating the base image boxRepeats times along the
// box's local x, y, and z
uniform bool bBoxProjection = false;
uniform vec3 boxRepeats = vec3(1.0f, 1.0f, 1.0f);
uniform LightSource lightSources[TOTAL_LIGHTS];
uniform Material material;
// the same matrix the vertex shader uses to turn normals with the shape
uniform mat3 normalMatrix;
// one of the NORMALS_ values above
uniform int normalSource = 0;
// true for the inside of the mug, whose meshes point their normals outward
uniform bool bFlipNormals = false;
// the share of light a surface reflects when seen straight on, used only by
// the glass desktop. 0 means the surface shows no reflection
uniform float reflectance = 0.0f;
// for a glowing part drawn unlit, such as the lamp's lit mouth: its middle is
// this many times as bright as its own color, fading to that color at its
// rim. 1 means an even glow
uniform float glowCoreGain = 1.0f;
// one of the MAPPING_ values above. For a tapered side, taperSize holds the
// shape's radius at its wide end and its length, in world units
uniform int surfaceMapping = 0;
uniform vec2 taperSize = vec2(1.0f, 1.0f);

// function prototypes
vec2 SurfaceCoordinate();
vec2 UnrolledCoordinate(float turn);
vec2 SphereCoordinate(float longitude);
void SeamGradients(vec2 uv, vec2 uvOther, out vec2 dx, out vec2 dy);
vec4 SampleTexture();
vec3 SurfaceNormal();
void AddLightSource(LightSource light, vec3 normal, vec3 viewDirection,
   inout vec3 ambient, inout vec3 diffuse, inout vec3 specular);

void main()
{
   // where objects rest on the glass, the mirrored scene's lowest surfaces
   // lie closer to the glass than the depth buffer can tell apart, and would
   // show through it as bright specks. Real surfaces are never below the
   // glass, so mirrored fragments that close to it are left out
   if ((fragmentPosition.y < 0.0f) && (fragmentPosition.y > -0.001f))
   {
      discard;
   }

   // the surface's own color: its texture, or its flat color
   vec4 baseColor = objectColor;
   if (bUseTexture == true)
   {
      baseColor = SampleTexture();
   }

   // the overlay covers the base in proportion to its alpha, so its
   // transparent background leaves the base showing and its soft edges
   // blend into it. Both are lit together below
   if (bUseOverlay == true)
   {
      vec4 overlay = texture(overlayTexture, fragmentTextureCoordinate * overlayUVscale);
      baseColor.rgb = mix(baseColor.rgb, overlay.rgb, overlay.a);
   }

   if (bUseLighting == false)
   {
      // the course cylinder's cap lies on a radius 1 disc of the unscaled
      // mesh, so its distance from the middle runs 0 to 1, and the glow eases
      // from the bright middle to the part's own color at the rim. A bright
      // light source exceeds what a color can show, so each channel stops at 1
      if (glowCoreGain > 1.0f)
      {
         float fromMiddle = clamp(length(fragmentMeshPosition.xz), 0.0f, 1.0f);
         float fade = smoothstep(0.0f, 1.0f, fromMiddle);
         float gain = mix(glowCoreGain, 1.0f, fade);
         baseColor.rgb = min(baseColor.rgb * gain, vec3(1.0f));
      }
      outFragmentColor = baseColor;
      return;
   }

   vec3 normal = SurfaceNormal();
   vec3 viewDirection = normalize(viewPosition - fragmentPosition);

   // add up the light that reaches this fragment from every light source
   vec3 ambient = vec3(0.0f);
   vec3 diffuse = vec3(0.0f);
   vec3 specular = vec3(0.0f);
   for (int i = 0; i < TOTAL_LIGHTS; i++)
   {
      AddLightSource(lightSources[i], normal, viewDirection, ambient, diffuse, specular);
   }

   // ambient and diffuse light take on the surface's own color. Specular
   // highlights keep the light's color, as they do on glaze, plastic, and
   // glass, so they are added after the surface color is applied
   vec3 surfaceLight = (ambient + diffuse) * baseColor.rgb;

   if (reflectance > 0.0f)
   {
      // Schlick's approximation of the Fresnel effect: a glossy surface
      // reflects the share given by reflectance when seen straight on, and
      // more as the view tilts toward the surface, until it reflects
      // everything at a grazing angle
      float facing = max(dot(normal, viewDirection), 0.0f);
      float fresnel = reflectance + (1.0f - reflectance) * pow(1.0f - facing, 5.0f);

      // the reflection is already drawn beneath this surface. The output is
      // premultiplied by its opacity: the surface's own light keeps
      // 1 - fresnel of its strength, the highlights are added in full, and
      // blending keeps fresnel of the reflection underneath
      outFragmentColor = vec4(surfaceLight * (1.0f - fresnel) + specular, 1.0f - fresnel);
   }
   else
   {
      outFragmentColor = vec4(surfaceLight + specular, baseColor.a);
   }
}

/***********************************************************
 *  SurfaceCoordinate()
 *
 *  Returns the coordinate the base texture is sampled at.
 *  Normally that is the mesh's own texture coordinate times
 *  UVscale. The course box maps the whole image onto every
 *  face, which stretches it on any face that is not square,
 *  so with bBoxProjection each face is found from which
 *  local coordinate sits on the box's surface, at -0.5 or
 *  0.5, and the image is laid across that face from its
 *  other two coordinates, repeating boxRepeats times along
 *  each axis. Each face reads upright from outside the box.
 *  A cylinder cap, tapered side, or the sphere may instead
 *  take its coordinate from its position; see
 *  surfaceMapping.
 ***********************************************************/
vec2 SurfaceCoordinate()
{
   if (surfaceMapping == MAPPING_CYLINDER_CAP)
   {
      // the cap's stored coordinates are typed with errors up to 0.02 on thin
      // fan triangles, so take the layout they approximate from the
      // position on the radius 1 disc instead
      vec3 p = fragmentMeshPosition;
      return(vec2(0.5f + 0.5f * p.z, 0.5f + 0.5f * p.x) * UVscale);
   }
   if (surfaceMapping == MAPPING_TAPERED_SIDE)
   {
      vec3 p = fragmentMeshPosition;
      return(UnrolledCoordinate(atan(-p.z, -p.x)));
   }
   if (surfaceMapping == MAPPING_SPHERE)
   {
      vec3 p = fragmentMeshPosition;
      return(SphereCoordinate((length(p.xz) > 0.000001f) ? atan(p.x, p.z) : 0.0f));
   }

   if (bBoxProjection == false)
   {
      return(fragmentTextureCoordinate * UVscale);
   }

   vec3 p = fragmentMeshPosition;
   vec3 a = abs(p);
   vec2 face;
   vec2 repeats;
   if ((a.y >= a.x) && (a.y >= a.z))
   {
      // top and bottom: across the width, and from front to back
      face = vec2(p.x + 0.5f, (p.y > 0.0f) ? (0.5f - p.z) : (p.z + 0.5f));
      repeats = boxRepeats.xz;
   }
   else if (a.x >= a.z)
   {
      // left and right sides: across the depth, and up the height
      face = vec2((p.x > 0.0f) ? (0.5f - p.z) : (p.z + 0.5f), p.y + 0.5f);
      repeats = boxRepeats.zy;
   }
   else
   {
      // front and back: across the width, and up the height
      face = vec2((p.z > 0.0f) ? (p.x + 0.5f) : (0.5f - p.x), p.y + 0.5f);
      repeats = boxRepeats.xy;
   }
   return(face * repeats);
}

/***********************************************************
 *  UnrolledCoordinate()
 *
 *  The course tapered cylinder is the wide part of a cone
 *  whose tip is at y = 2 of the unscaled mesh. Unrolled
 *  flat, that cone is a slice of a disc: each point sits at
 *  its slant distance from the tip, at its angle around the
 *  axis times the sine of the cone's half angle, so the
 *  image keeps square texels. turn is the angle around the
 *  axis, measured from the mesh's -X.
 ***********************************************************/
vec2 UnrolledCoordinate(float turn)
{
   float slantLength = sqrt(taperSize.x * taperSize.x + 4.0f * taperSize.y * taperSize.y);
   float sinHalfAngle = taperSize.x / slantLength;
   // the slant distance falls evenly from slantLength at y = 0 to 0 at y = 2
   float fromTip = 0.5f * (2.0f - fragmentMeshPosition.y) * slantLength;
   float angle = turn * sinHalfAngle;
   // UVscale.x repeats fit around the wide end, 2 pi times its radius long
   float repeatsPerUnit = UVscale.x / (6.2831853f * taperSize.x);
   return(fromTip * repeatsPerUnit * vec2(cos(angle), sin(angle)));
}

/***********************************************************
 *  SphereCoordinate()
 *
 *  For the course sphere: longitude across and latitude up,
 *  the layout a 2:1 image is drawn for. Texels keep square
 *  corners everywhere and narrow toward the poles. v is the
 *  course mesh's own (0.5 plus latitude over pi), so a line
 *  drawn across the image still lands on the same latitude.
 *  longitude is measured from the mesh's +Z toward +X.
 ***********************************************************/
vec2 SphereCoordinate(float longitude)
{
   vec3 p = normalize(fragmentMeshPosition);
   float latitude = asin(clamp(p.y, -1.0f, 1.0f));
   return(vec2(0.5f + longitude / 6.2831853f, 0.5f + latitude / 3.1415927f) * UVscale);
}

/***********************************************************
 *  SeamGradients()
 *
 *  Returns the screen-space change of a texture coordinate
 *  that jumps at a seam. uvOther is the same coordinate
 *  measured with its jump somewhere else. Where one of the
 *  two jumps its change is huge, so the smaller one is used,
 *  and the seam does not pick a tiny mipmap.
 ***********************************************************/
void SeamGradients(vec2 uv, vec2 uvOther, out vec2 dx, out vec2 dy)
{
   dx = dFdx(uv);
   dy = dFdy(uv);
   vec2 dxOther = dFdx(uvOther);
   vec2 dyOther = dFdy(uvOther);
   if (dot(dxOther, dxOther) + dot(dyOther, dyOther) < dot(dx, dx) + dot(dy, dy))
   {
      dx = dxOther;
      dy = dyOther;
   }
}

/***********************************************************
 *  SampleTexture()
 *
 *  Returns the base texture's color. The sphere's longitude
 *  jumps at the back, where the image's edges meet, so its
 *  screen-space change comes from SeamGradients(). The
 *  unrolled tapered side has a seam on the mesh's +X side,
 *  where the course meshes also put theirs: there the angle
 *  jumps and the slice's two edges meet. Near it, the image
 *  is read at both edges and mixed evenly at the seam.
 ***********************************************************/
vec4 SampleTexture()
{
   vec2 dx;
   vec2 dy;
   vec3 p = fragmentMeshPosition;

   if (surfaceMapping == MAPPING_SPHERE)
   {
      // the same longitude measured from -Z, plus pi, jumps at the front
      vec2 uvSphere = SurfaceCoordinate();
      SeamGradients(uvSphere, SphereCoordinate(atan(-p.x, -p.z) + 3.1415927f), dx, dy);
      return(textureGrad(objectTexture, uvSphere, dx, dy));
   }
   if (surfaceMapping != MAPPING_TAPERED_SIDE)
   {
      return(texture(objectTexture, SurfaceCoordinate()));
   }

   float turn = atan(-p.z, -p.x);
   vec2 uv = UnrolledCoordinate(turn);
   // the same point measured from +X, plus pi, jumps on the -X side instead
   SeamGradients(uv, UnrolledCoordinate(atan(p.z, p.x) + 3.1415927f), dx, dy);

   // within 20 degrees either side of the seam, fade toward the same point
   // read from the slice's other edge, reaching an even mix at the seam
   float otherTurn = turn - sign(turn) * 6.2831853f;
   float blend = 0.5f * smoothstep(3.1415927f - 0.35f, 3.1415927f, abs(turn));
   vec4 here = textureGrad(objectTexture, uv, dx, dy);
   vec4 there = textureGrad(objectTexture, UnrolledCoordinate(otherTurn), dx, dy);
   return(mix(here, there, blend));
}

/***********************************************************
 *  SurfaceNormal()
 *
 *  Returns the unit normal of the surface at this fragment,
 *  in world space. Three course meshes store normals that
 *  are off for lighting: the cylinder's side normals step
 *  around the side in facets, and six of them tilt upward;
 *  the cone's side normals lie flat although its side slopes
 *  at 45 degrees; and the tapered cylinder gives each facet
 *  of its side one flat normal. For those three shapes the
 *  normal is rebuilt from the fragment's position on the
 *  unscaled mesh. Every other mesh, including the sphere and
 *  the project's bent tubes, which store exact normals, uses
 *  its stored normal.
 ***********************************************************/
vec3 SurfaceNormal()
{
   vec3 meshPoint = fragmentMeshPosition;
   vec3 normal;

   if (normalSource == NORMALS_CYLINDER_SIDE)
   {
      // the side of the course cylinder is a radius 1 tube around the Y
      // axis, so its normal points straight out from that axis
      normal = normalMatrix * vec3(meshPoint.x, 0.0f, meshPoint.z);
   }
   else if (normalSource == NORMALS_CONE_SIDE)
   {
      // the course cone narrows from radius 1 at y = 0 to a point at
      // y = 1. Its side rises one unit for each unit it narrows, so the
      // normal at radius r leans up by as much as it points out:
      // (x / r, 1, z / r), which points the same way as (x, r, z). The
      // tip itself has no single normal, so it faces straight up
      float radius = length(meshPoint.xz);
      if (radius > 0.0001f)
      {
         normal = normalMatrix * vec3(meshPoint.x, radius, meshPoint.z);
      }
      else
      {
         normal = normalMatrix * vec3(0.0f, 1.0f, 0.0f);
      }
   }
   else if (normalSource == NORMALS_TAPERED_SIDE)
   {
      // the course tapered cylinder narrows from radius 1 at y = 0 to 0.5
      // at y = 1, so its side leans in half a unit for each unit it rises
      // and its normal at radius r is (x / r, 0.5, z / r), which points
      // the same way as (x, 0.5 r, z)
      float radius = length(meshPoint.xz);
      normal = normalMatrix * vec3(meshPoint.x, 0.5f * radius, meshPoint.z);
   }
   else
   {
      normal = fragmentVertexNormal;
   }

   normal = normalize(normal);
   if (bFlipNormals == true)
   {
      normal = -normal;
   }
   return(normal);
}

/***********************************************************
 *  AddLightSource()
 *
 *  Adds one light source's ambient, diffuse, and specular
 *  light at this fragment to the running totals.
 ***********************************************************/
void AddLightSource(LightSource light, vec3 normal, vec3 viewDirection,
   inout vec3 ambient, inout vec3 diffuse, inout vec3 specular)
{
   if (light.type == LIGHT_OFF)
   {
      return;
   }

   // ambient light arrives from every direction, so it also reaches the
   // sides that face away from the light
   ambient += light.ambientColor * material.ambientColor * material.ambientStrength;

   // the direction from this fragment toward the light, and the share of
   // the light's strength that is left when it arrives
   vec3 lightDirection;
   float strength = 1.0f;
   if (light.type == LIGHT_DIRECTIONAL)
   {
      // a directional light is so far away that its rays are parallel and
      // it does not weaken across the scene. Its direction is the way its
      // light travels
      lightDirection = normalize(-light.direction);
   }
   else
   {
      vec3 toLight = light.position - fragmentPosition;
      float lightDistance = length(toLight);
      lightDirection = toLight / lightDistance;

      // a point light or spotlight weakens with distance
      strength = 1.0f / (light.constant + light.linear * lightDistance +
         light.quadratic * lightDistance * lightDistance);

      if (light.type == LIGHT_SPOT)
      {
         // a spotlight shines fully inside its inner cone, fades out
         // between the inner and outer cones, and gives no light outside.
         // theta is the cosine of the angle between the spotlight's aim and
         // the ray to this fragment, so it grows toward the middle of the beam
         float theta = dot(-lightDirection, normalize(light.direction));
         float edge = (theta - light.outerConeCos) / (light.innerConeCos - light.outerConeCos);
         strength *= clamp(edge, 0.0f, 1.0f);
      }
   }

   // diffuse light is strongest where the surface faces the light directly
   float impact = max(dot(normal, lightDirection), 0.0f);
   diffuse += strength * impact * light.diffuseColor * material.diffuseColor;

   // a specular highlight appears where the light reflects toward the
   // viewer. A higher shininess makes the highlight smaller and sharper.
   // Surfaces facing away from the light get no highlight
   if (impact > 0.0f)
   {
      vec3 reflectDirection = reflect(-lightDirection, normal);
      float highlight = pow(max(dot(viewDirection, reflectDirection), 0.0f), material.shininess);
      specular += strength * highlight * light.specularIntensity *
         light.specularColor * material.specularColor;
   }
}
