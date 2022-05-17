#include "../common.glsl"

#define DEBUG_DRAW 0

#if DEBUG_DRAW
#define DEBUG_DRAW_LOC   20
#define MAX_DEBUG_COLORS 10
const vec3 debugColors[MAX_DEBUG_COLORS] = {
  vec3(1,1,1),
  vec3(1,0,0),
  vec3(0,1,0),
  vec3(0,0,1),
  vec3(1,1,0),
  vec3(1,0,1),
  vec3(0,1,1),
  vec3(1,0.5,0),
  vec3(0.5,1,0),
  vec3(0,0.5,1),
  };
#endif

#define MAX_NUM_SKELETAL_NODES 96
#define MAX_MORPH_LAYERS       3

#define T_LANDSCAPE 0
#define T_OBJ       1
#define T_SKINING   2
#define T_MORPH     3
#define T_PFX       4

#define L_Diffuse  0
#define L_Shadow0  1
#define L_Shadow1  2
#define L_Scene    3
#define L_Matrix   4
#define L_Material 5
#define L_GDiffuse 6
#define L_GDepth   7
#define L_Ibo      8
#define L_Vbo      9
#define L_MeshDesc 10
#define L_MorphId  11
#define L_Morph    12

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING || MESH_TYPE==T_MORPH)
#define LVL_OBJECT 1
#endif

#if (defined(VERTEX) || defined(TESSELATION)) && (defined(LVL_OBJECT) || defined(WATER))
#define MAT_ANIM 1
#endif

#if !defined(SHADOW_MAP) && (MESH_TYPE==T_PFX)
#define MAT_COLOR 1
#endif

struct Varyings {
  vec4 scr;
  vec2 uv;

#if !defined(SHADOW_MAP)
  vec4 shadowPos[2];
  vec3 normal;
#endif

#if !defined(SHADOW_MAP) || defined(WATER)
  vec3 pos;
#endif

#if defined(MAT_COLOR)
  vec4 color;
#endif
  };

struct Light {
  vec4  pos;
  vec3  color;
  float range;
  };

struct MorphDesc {
  uint  indexOffset;
  uint  sample0;
  uint  sample1;
  uint  alpha16_intensity16;
  };

#if (MESH_TYPE==T_OBJ || MESH_TYPE==T_SKINING)
layout(push_constant, std430) uniform UboPush {
  uint      baseInstance;
  float     fatness;
  } push;
#elif (MESH_TYPE==T_MORPH)
layout(push_constant, std430) uniform UboPush {
  uint      baseInstance;
  float     fatness;
  float     padd1;
  float     padd2;
  MorphDesc morph[MAX_MORPH_LAYERS];
  } push;
#elif (MESH_TYPE==T_PFX || MESH_TYPE==T_LANDSCAPE)
// no push
#else
#error "unknown MESH_TYPE"
#endif

#if defined(FRAGMENT) && !(defined(SHADOW_MAP) && !defined(ATEST))
layout(binding = L_Diffuse) uniform sampler2D textureD;
#endif

#if defined(FRAGMENT) && !defined(SHADOW_MAP)
layout(binding = L_Shadow0) uniform sampler2D textureSm0;
layout(binding = L_Shadow1) uniform sampler2D textureSm1;
#endif

layout(binding = L_Scene, std140) uniform UboScene {
  vec3  ldir;
  float shadowSize;
  mat4  viewProject;
  mat4  viewProjectInv;
  mat4  shadow[2];
  vec3  ambient;
  vec4  sunCl;
  vec4  frustrum[6];
  vec3  clipInfo;
  // float padd0;
  vec3  camPos;
  } scene;

#if defined(LVL_OBJECT) && (defined(VERTEX) || defined(MESH))
layout(binding = L_Matrix, std140) readonly buffer UboAnim {
  mat4 pos[];
  } matrix;
#endif

#if defined(MAT_ANIM)
layout(binding = L_Material, std140) uniform UboMaterial {
  vec2  texAnim;
  float waveAnim;
  float waveMaxAmplitude;
  } material;
#endif

#if defined(FRAGMENT) && (defined(WATER) || defined(GHOST))
layout(binding = L_GDiffuse) uniform sampler2D gbufferDiffuse;
layout(binding = L_GDepth  ) uniform sampler2D gbufferDepth;
#endif

#if (MESH_TYPE==T_MORPH) && defined(VERTEX)
layout(binding = L_MorphId, std430) readonly buffer SsboMorphId {
  int  index[];
  } morphId;
layout(binding = L_Morph, std430) readonly buffer SsboMorph {
  vec4 samples[];
  } morph;
#endif