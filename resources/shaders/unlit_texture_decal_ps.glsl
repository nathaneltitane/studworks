SW_PIXEL_INPUT vec2 PixelTexCoord;
SW_PIXEL_INPUT vec3 PixelCutaway;
SW_PIXEL_OUTPUT

uniform mediump vec4 MaterialColor;
uniform sampler2D Texture;
uniform mediump int CutawayShape;

void main()
{
	SW_PIXEL_CUTAWAY
	SW_SHADER_PRECISION vec4 TexelColor = texture2D(Texture, PixelTexCoord);
	gl_FragColor = mix(MaterialColor, TexelColor, TexelColor.a);
}
