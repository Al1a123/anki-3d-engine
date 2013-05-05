#pragma anki include "shaders/CommonFrag.glsl"

//==============================================================================
// Variables                                                                   =
//==============================================================================

/// @name Varyings
/// @{
#define vTexCoords_DEFINED
in vec2 vTexCoords;

#if defined(PASS_COLOR)
in vec3 vNormal;
#	define vNormal_DEFINED
in vec3 vTangent;
#	define vTangent_DEFINED
in float vTangentW;
#	define vTangentW_DEFINED
in vec3 vVertPosViewSpace;
#	define vVertPosViewSpace_DEFINED
flat in float vSpecularComponent;
#	define vSpecularComponent_DEFINED
#endif
/// @}

/// @name Fragment out
/// @{
#if defined(PASS_COLOR)
layout(location = 0) out uvec2 fMsFai0;
#	define fMsFai0_DEFINED
#endif
/// @}

//==============================================================================
// Functions                                                                   =
//==============================================================================

#pragma anki include "shaders/Pack.glsl"
#pragma anki include "shaders/MsBsCommon.glsl"

/// @param[in] normal The fragment's normal in view space
/// @param[in] tangent The tangent
/// @param[in] tangent Extra stuff for the tangent
/// @param[in] map The map
/// @param[in] texCoords Texture coordinates
#if defined(PASS_COLOR)
#	define getNormalFromTexture_DEFINED
vec3 getNormalFromTexture(in vec3 normal, in vec3 tangent, in float tangentW,
	in sampler2D map, in vec2 texCoords)
{
#	if LOD > 0
	return normalize(normal);
#	else
	vec3 n = normalize(normal);
	vec3 t = normalize(tangent);
	vec3 b = cross(n, t) * tangentW;

	mat3 tbnMat = mat3(t, b, n);

	vec3 nAtTangentspace = (texture(map, texCoords).rgb - 0.5) * 2.0;

	return normalize(tbnMat * nAtTangentspace);
#	endif
}
#endif

/// Just normalize
#if defined(PASS_COLOR)
#	define getNormalSimple_DEFINED
vec3 getNormalSimple(in vec3 normal)
{
	return normalize(normal);
}
#endif

/// Environment mapping calculations
/// @param[in] vertPosViewSpace Fragment position in view space
/// @param[in] normal Fragment's normal in view space as well
/// @param[in] map The env map
/// @return The color
#if defined(PASS_COLOR)
#	define getEnvironmentColor_DEFINED
vec3 getEnvironmentColor(in vec3 vertPosViewSpace, in vec3 normal,
	in sampler2D map)
{
	// In case of normal mapping I could play with vertex's normal but this 
	// gives better results and its allready computed
	
	vec3 u = normalize(vertPosViewSpace);
	vec3 r = reflect(u, normal);
	r.z += 1.0;
	float m = 2.0 * length(r);
	vec2 semTexCoords = r.xy / m + 0.5;

	vec3 semCol = texture(map, semTexCoords).rgb;
	return semCol;
}
#endif

/// Using a 4-channel texture and a tolerance discard the fragment if the 
/// texture's alpha is less than the tolerance
/// @param[in] map The diffuse map
/// @param[in] tolerance Tolerance value
/// @param[in] texCoords Texture coordinates
/// @return The RGB channels of the map
#define getDiffuseColorAndDoAlphaTesting_DEFINED
vec3 getDiffuseColorAndDoAlphaTesting(
	in sampler2D map,
	in vec2 texCoords,
	in float tolerance)
{
#if defined(PASS_COLOR)
	vec4 col = texture(map, texCoords);
	if(col.a < tolerance)
	{
		discard;
	}
	return col.rgb;
#else // Depth
#	if LOD > 0
	return vec3(0.0);
#	else
	float a = texture(map, texCoords).a;
	if(a < tolerance)
	{
		discard;
	}
	return vec3(0.0);
#	endif
#endif
}

/// Just read the RGB color from texture
#if defined(PASS_COLOR)
#	define readRgbFromTexture_DEFINED
vec3 readRgbFromTexture(in sampler2D tex, in vec2 texCoords)
{
	return texture(tex, texCoords).rgb;
}
#endif

/// Write the data to FAIs
#if defined(PASS_COLOR)
#	define writeFais_DEFINED
void writeFais(
	in vec3 diffCol, // from 0 to 1
	in vec3 normal, 
	in float specularComponent, // Streangth and shininess
	in float blurring)
{
	// Diffuse color and specular
	fMsFai0[0] = packUnorm4x8(vec4(diffCol, specularComponent));
	// Normal
	fMsFai0[1] = packHalf2x16(packNormal(normal));
}
#endif

/// Write the data to FAIs
#if defined(PASS_COLOR)
#	define writeFaisPackSpecular_DEFINED
void writeFaisPackSpecular(
	in vec3 diffCol, // Normalized
	in vec3 normal, 
	in vec2 specular, // Streangth and shininess
	in float blurring)
{
	writeFais(diffCol, normal, packSpecular(specular), blurring);
}
#endif