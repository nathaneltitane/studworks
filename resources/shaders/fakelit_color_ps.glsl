SW_PIXEL_INPUT vec3 PixelPosition;
SW_PIXEL_INPUT vec3 PixelNormal;
SW_PIXEL_INPUT vec3 PixelCutaway;
SW_PIXEL_OUTPUT

uniform mediump vec4 MaterialColor;
uniform mediump vec3 LightPosition;
uniform mediump vec3 EyePosition;
uniform mediump int LightingStyle;
uniform mediump int CutawayShape;
uniform mediump mat4 WorldCutawayMatrix;
uniform mediump vec4 CutawayEye;

void main()
{
	SW_PIXEL_CUTAWAY
	SW_PIXEL_CUTAWAY_CAP
	SW_PIXEL_FAKE_LIGHTING
	SW_SHADER_PRECISION vec3 DiffuseColor = MaterialColor.rgb * Diffuse;
	gl_FragColor = vec4(DiffuseColor + SpecularColor, MaterialColor.a);
}
