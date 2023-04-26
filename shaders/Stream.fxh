#pragma once

#include "ReShade.fxh"

#define STREAM(NAME, SHADER, FORMAT) \
  namespace NAME {                                                                                                      \
      texture NAME { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = FORMAT; };                                  \
      sampler Preview { Texture = NAME; };                                                                              \
      uniform bool bUIPreview < ui_label = "Preview"; ui_tooltip = "Draw the stream to the screen."; > = false; \
      float3 PS_Preview(float4 position : SV_Position, float2 texcoord : TEXCOORD) : SV_Target {                        \
          if (bUIPreview) return tex2D(Preview, texcoord).rgb;                                                          \
          return tex2D(ReShade::BackBuffer, texcoord).rgb;                                                              \
      }                                                                                                                 \
      technique NAME {                                                                                                  \
          pass stream  { VertexShader = PostProcessVS; PixelShader = SHADER; RenderTarget = NAME; }                     \
          pass preview { VertexShader = PostProcessVS; PixelShader = PS_Preview; }                                      \
      }                                                                                                                 \
  }                                                                                                                     \

