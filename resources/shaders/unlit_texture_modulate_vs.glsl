SW_VERTEX_INPUT vec3 VertexPosition;
SW_VERTEX_INPUT vec2 VertexTexCoord;
SW_VERTEX_OUTPUT vec2 PixelTexCoord;

uniform mat4 WorldViewProjectionMatrix;

void main()
{
	gl_Position = WorldViewProjectionMatrix * vec4(VertexPosition, 1.0);
	PixelTexCoord = VertexTexCoord;
}
