SW_VERTEX_INPUT vec3 VertexPosition;
SW_VERTEX_INPUT vec3 VertexTexCoord;
SW_VERTEX_OUTPUT vec3 PixelNormal;
SW_VERTEX_OUTPUT vec3 PixelFaceCoord;

uniform mat4 WorldViewProjectionMatrix;

void main()
{
	PixelNormal = normalize(VertexPosition);
	PixelFaceCoord = VertexTexCoord;
	gl_Position = WorldViewProjectionMatrix * vec4(VertexPosition, 1.0);
}
