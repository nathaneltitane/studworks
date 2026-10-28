SW_PIXEL_INPUT vec3 PixelNormal;
SW_PIXEL_INPUT vec3 PixelFaceCoord;
SW_PIXEL_OUTPUT

uniform mediump vec4 HighlightParams[4];
uniform sampler2D Texture;

void main()
{
	SW_SHADER_PRECISION float Distance = length(vec3(HighlightParams[0]) - PixelNormal);
	SW_SHADER_PRECISION float Highlight = step(Distance, HighlightParams[0].w);
	SW_SHADER_PRECISION vec4 SurfaceColor = mix(HighlightParams[2], HighlightParams[3], Highlight);
	SW_SHADER_PRECISION vec2 FaceUV = PixelFaceCoord.xy;
	SW_SHADER_PRECISION float LabelMask = step(0.1, FaceUV.x) * step(FaceUV.x, 0.9) * step(0.3, FaceUV.y) * step(FaceUV.y, 0.7);
	SW_SHADER_PRECISION vec2 SampleUV = clamp(FaceUV, vec2(0.1, 0.3), vec2(0.9, 0.7));
	SW_SHADER_PRECISION vec2 AtlasUV = vec2(SampleUV.x, (PixelFaceCoord.z * 64.0 + SampleUV.y * 128.0 - 32.0) / 512.0);
	SW_SHADER_PRECISION float TextFill = LabelMask * smoothstep(0.1, 0.7, texture2D(Texture, AtlasUV, -0.5).a) * HighlightParams[1].a;
	gl_FragColor = vec4(mix(SurfaceColor.rgb, HighlightParams[1].rgb, TextFill), SurfaceColor.a);
}
