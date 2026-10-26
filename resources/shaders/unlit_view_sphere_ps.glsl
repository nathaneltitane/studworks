SW_PIXEL_INPUT vec3 PixelNormal;
SW_PIXEL_INPUT vec3 PixelFaceCoord;
SW_PIXEL_OUTPUT

uniform mediump vec4 HighlightParams[4];
uniform mediump vec4 TextHaloColor;
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
	SW_SHADER_PRECISION float TextDistance = texture2D(Texture, AtlasUV).a;
	SW_SHADER_PRECISION float TextEdge = max(fwidth(TextDistance), 0.003);
	SW_SHADER_PRECISION float TextFill = smoothstep(0.5 - TextEdge, 0.5 + TextEdge, TextDistance);
	SW_SHADER_PRECISION float TextOutline = smoothstep(0.375 - TextEdge, 0.375 + TextEdge, TextDistance);
	SW_SHADER_PRECISION float Opacity = LabelMask * max(TextFill * HighlightParams[1].a, TextOutline * TextHaloColor.a);
	SW_SHADER_PRECISION vec3 InkColor = mix(TextHaloColor.rgb, HighlightParams[1].rgb, TextFill);
	gl_FragColor = vec4(mix(SurfaceColor.rgb, InkColor, Opacity), SurfaceColor.a);
}
