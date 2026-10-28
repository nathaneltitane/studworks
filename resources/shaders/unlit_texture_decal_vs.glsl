SW_VERTEX_INPUT vec3 VertexPosition;
SW_VERTEX_INPUT vec2 VertexTexCoord;
SW_VERTEX_OUTPUT vec2 PixelTexCoord;
SW_VERTEX_OUTPUT vec3 PixelCutaway;

uniform mat4 WorldViewProjectionMatrix;
uniform mat4 CutawayMatrix;

void main()
{
	PixelCutaway = (CutawayMatrix * vec4(VertexPosition, 1.0)).xyz;
	gl_Position = WorldViewProjectionMatrix * vec4(VertexPosition, 1.0);
	PixelTexCoord = VertexTexCoord;
}
