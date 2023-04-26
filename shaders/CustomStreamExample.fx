#include "Stream.fxh"

// Example of how to create a custom stream.

// First, define a shader that outputs the image you want to capture.
// See https://github.com/crosire/reshade-shaders/blob/master/REFERENCE.md
float4 PS_Stream(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    return float4(tex2D(ReShade::BackBuffer, 1 - texcoord).rgb, 1.0);
}

// Then, use the 'STREAM' macro from 'Stream.fxh' to generate the rest.
// Arguments: name = STREAM_Example, shader = PS_Stream, pixel_format = RGBA8
STREAM(STREAM_Example, PS_Stream, RGBA8);

// Alternatively, check out 'Stream.fxh' for how it implement it yourself.
