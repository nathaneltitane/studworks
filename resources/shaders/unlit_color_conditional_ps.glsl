SW_PIXEL_INPUT vec3 PixelCutaway;
SW_PIXEL_OUTPUT

uniform mediump vec4 MaterialColor;
uniform mediump int CutawayShape;

void main()
{
	SW_PIXEL_CUTAWAY
	gl_FragColor = MaterialColor;
}
