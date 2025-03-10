//
// FOnline default effect
// For sprites
//

#include "IOStructures.inc"

sampler2D ColorMap;
sampler2D EggMap;


// Vertex shader
AppToVsToPs_2DEgg VSSimple(AppToVsToPs_2DEgg input)
{
	// Just pass forward
	return input;
}


// Pixel shader
float4 PSSimple(AppToVsToPs_2DEgg input) : COLOR
{
	float4 output;

	// Sample
	float4 texColor = tex2D(ColorMap, input.TexCoord);
	output.rgb = (texColor.rgb * input.Diffuse.rgb) * 2;
	output.a = texColor.a * input.Diffuse.a;

	// Egg transparent
	if(input.TexEggCoord.x != 0.0f) output.a *= tex2D(EggMap, input.TexEggCoord).a;

	if(output.r>0.5 && output.r<0.9)
	{
		output.r+=0.2;
	}
	else if(output.r>0.1 && output.r<0.5)
	{
		output.r-=0.1;
	}

	if(output.g>0.5 && output.g<0.9)
	{
		output.g+=0.2;
	}
	else if(output.g>0.1 && output.g<0.5)
	{
		output.g-=0.1;
	}

	if(output.b>0.5 && output.b<0.9)
	{
		output.b+=0.2;
	}
	else if(output.b>0.1 && output.b<0.5)
	{
		output.b-=0.05;
	}
	if(output.r<0.1 && output.g<0.1 && output.b<0.1)
	{
		output.r=0.0f;
		output.g=0.0f;
		output.b=0.0f;
	}

	return output;
}


// Techniques
technique Simple
{
	pass p0
	{
		VertexShader = (compile vs_2_0 VSSimple());
		PixelShader  = (compile ps_2_0 PSSimple());
	}
}

