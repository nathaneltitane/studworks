SW_VERTEX_INPUT vec3 VertexPosition;
SW_VERTEX_INPUT vec3 VertexNormal;
SW_VERTEX_OUTPUT vec3 PixelPosition;
SW_VERTEX_OUTPUT vec3 PixelNormal;
SW_VERTEX_OUTPUT vec3 PixelCutaway;

uniform mat4 WorldViewProjectionMatrix;
uniform mat4 WorldMatrix;
uniform mat4 CutawayMatrix;

void main()
{
	PixelCutaway = (CutawayMatrix * vec4(VertexPosition, 1.0)).xyz;
	PixelPosition = (WorldMatrix * vec4(VertexPosition, 1.0)).xyz;
	PixelNormal = (WorldMatrix * vec4(VertexNormal, 0.0)).xyz;
	gl_Position = WorldViewProjectionMatrix * vec4(VertexPosition, 1.0);
}
