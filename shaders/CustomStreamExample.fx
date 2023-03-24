#include "Stream.fxh"

// Define a shader for capturing desired buffers.
// See https://github.com/crosire/reshade-shaders/blob/master/REFERENCE.md
float4 PS_Stream(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
{
    return float4(tex2D(ReShade::BackBuffer, 1 - texcoord).rgb, 1.0);
}

// Use the 'STREAM' macro fom 'Stream.fxh' to generate the rest.
// Arguments: name = STREAM_Example, shader = PS_Stream, pixel_format = RGBA8
STREAM(STREAM_Example, PS_Stream, RGBA8);
