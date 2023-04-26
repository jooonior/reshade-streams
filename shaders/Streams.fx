#include "ReShade.fxh"
#include "Macros.fxh"

// "Inspired" by: https://github.com/crosire/reshade-shaders/blob/slim/Shaders/DisplayDepth.fx

namespace Streams {

    texture STREAM_Color { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
    texture STREAM_Depth { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
    texture STREAM_Normals { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

    sampler Preview_Color { Texture = STREAM_Color; };
    sampler Preview_Depth { Texture = STREAM_Depth; };
    sampler Preview_Normals { Texture = STREAM_Normals; };

    // Preview Options.
    UI_COMBO(iUIPreview, "Preview", "Draw the selected stream to the screen.", 0, 3, 0, "Off\0Color\0Depth\0Normals\0")

    // Depth Options.
    CAT_BOOL(bUIRgbDepth, "Depth Options", "Full RGB", "Display depth using full RGB spectrum.", false)
    CAT_BOOL(bUIDither, "Depth Options", "Dither", "Dither to simulate finer gradients.", false)
    CAT_FLOAT_D(fUIInBlack, "Depth Options", "In Black", "Limit input/output values (levels).", 0.0, 1.0, 0.0)
    CAT_FLOAT_D(fUIInWhite, "Depth Options", "In White", "Limit input/output values (levels).", 0.0, 1.0, 1.0)
    CAT_FLOAT_D(fUIOutBlack, "Depth Options", "Out Black", "Limit input/output values (levels).", 0.0, 1.0, 0.0)
    CAT_FLOAT_D(fUIOutWhite, "Depth Options", "Out White", "Limit input/output values (levels).", 0.0, 1.0, 1.0)
    CAT_FLOAT_D(fUIGamma, "Depth Options", "Gamma", "Limit input/output values (levels).", 0.0, 100.0, 1.0)

    float ShiftLevels(float value, float in_black, float in_white, float out_black, float out_white, float gamma)
    {
        value = max(value - in_black, 0)/(in_white - in_black);
        value = pow(abs(value) , gamma);
        value = value * (out_white - out_black) + out_black;
        return value;
    }

    float3 Dither(float2 texcoord)
    {
        // Number of bits per channel.
        const float dither_bit = BUFFER_COLOR_BIT_DEPTH;

        // Calculate how big the shift should be.
        float dither_shift = 0.25 * (1.0 / (pow(2, dither_bit) - 1.0));

        // Shift the individual colors differently, thus making it even harder to see the dithering pattern.
        float3 dither_shift_RGB = float3(dither_shift, -dither_shift, dither_shift); // Subpixel dithering

        // Modify shift acording to grid position.
        float grid_position = frac(dot(texcoord, (BUFFER_SCREEN_SIZE * float2(1.0 / 16.0, 10.0 / 36.0)) + 0.25));
        dither_shift_RGB = lerp(2.0 * dither_shift_RGB, -2.0 * dither_shift_RGB, grid_position);

        return dither_shift_RGB;
    }

    float GetLinearizedDepth(float2 texcoord)
    {
        float depth = ReShade::GetLinearizedDepth(texcoord);

        depth = ShiftLevels(depth, fUIInBlack, fUIInWhite, fUIOutBlack, fUIOutWhite, fUIGamma);

        return depth;
    }

    float3 DepthToRgb(float depth)
    {
        depth *= 16777215;  // 2**24 - 1
        float b = depth % 256;
        depth = (depth - b) / 256;
        float g = depth % 256;
        depth = (depth - g) / 256;
        float r = depth;
        return float3(r, g, b) / 255;
    }

    float3 GetDepth(float2 texcoord)
    {
        float depth = ReShade::GetLinearizedDepth(texcoord);

        depth = ShiftLevels(depth, fUIInBlack, fUIInWhite, fUIOutBlack, fUIOutWhite, fUIGamma);

        float3 color;

        if (bUIRgbDepth) {
            color = DepthToRgb(depth);
        } else {
            color = depth.xxx;
        }

        if (bUIDither) {
            color += Dither(texcoord);
        }

        return color;
    }

    float3 GetScreenSpaceNormal(float2 texcoord)
    {
        float3 offset = float3(BUFFER_PIXEL_SIZE, 0.0);
        float2 posCenter = texcoord.xy;
        float2 posNorth  = posCenter - offset.zy;
        float2 posEast   = posCenter + offset.xz;

        float3 vertCenter = float3(posCenter - 0.5, 1) * ReShade::GetLinearizedDepth(posCenter);
        float3 vertNorth  = float3(posNorth  - 0.5, 1) * ReShade::GetLinearizedDepth(posNorth);
        float3 vertEast   = float3(posEast   - 0.5, 1) * ReShade::GetLinearizedDepth(posEast);

        return normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) * 0.5 + 0.5;
    }

    void PS_Stream(in float4 position : SV_Position, in float2 texcoord : TEXCOORD,
                   out float4 bbTgt : SV_Target0,
                   out float4 depthTgt : SV_Target1,
                   out float4 normalTgt : SV_Target2)
    {
        bbTgt = float4(tex2D(ReShade::BackBuffer, texcoord).rgb, 1.0);
        depthTgt = float4(GetDepth(texcoord).rgb, 1.0);
        normalTgt = float4(GetScreenSpaceNormal(texcoord).rgb, 1.0);
    }

    float3 PS_Preview(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target
    {
        switch (iUIPreview) {
        case 0:
        default:
            return tex2D(ReShade::BackBuffer, texcoord).rgb;
        case 1:
            return tex2D(Preview_Color, texcoord).rgb;
        case 2:
            return tex2D(Preview_Depth, texcoord).rgb;
        case 3:
            return tex2D(Preview_Normals, texcoord).rgb;
        }
    }

    technique Streams
    {
        pass stream {
            VertexShader = PostProcessVS;
            PixelShader = PS_Stream;
            RenderTarget0 = STREAM_Color;
            RenderTarget1 = STREAM_Depth;
            RenderTarget2 = STREAM_Normals;
        }
        pass preview {
            VertexShader = PostProcessVS;
            PixelShader = PS_Preview;
        }
    }
}

