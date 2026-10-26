SW_PIXEL_INPUT vec3 PixelPosition;
SW_PIXEL_INPUT vec3 PixelNormal;
SW_PIXEL_INPUT vec2 PixelTexCoord;
SW_PIXEL_OUTPUT

uniform mediump vec4 MaterialColor;
uniform mediump vec3 LightPosition;
uniform mediump vec3 EyePosition;
uniform sampler2D Texture;

void main()
{
	SW_PIXEL_FAKE_LIGHTING
	SW_SHADER_PRECISION vec4 TexelColor = texture2D(Texture, PixelTexCoord);
	SW_SHADER_PRECISION vec3 DiffuseColor = mix(MaterialColor.rgb, TexelColor.rgb, TexelColor.a);
	gl_FragColor = vec4(DiffuseColor * Diffuse + SpecularColor, max(TexelColor.a, MaterialColor.a));
}
