#include "PresentPass.h"
#include "RenderDevice.h"
#include "common/Log.h"

// Diligent includes
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/Sampler.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

namespace arcanee::render {

using namespace Diligent;

// Fullscreen triangle vertex shader (generates positions from vertex ID)
static const char *VSSource = R"(
struct VSOutput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

void main(in uint VertId : SV_VertexID, out VSOutput PSIn) {
    // Generate fullscreen triangle
    float2 UV = float2((VertId << 1) & 2, VertId & 2);
    PSIn.UV = UV;
    PSIn.Pos = float4(UV * 2.0 - 1.0, 0.0, 1.0);
    PSIn.Pos.y = -PSIn.Pos.y;  // Flip Y for correct orientation
}
)";

// Pixel shader that samples the CBUF texture
static const char *PSSource = R"(
Texture2D    g_Texture;
SamplerState g_Texture_sampler;

struct PSInput {
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
};

float4 main(in PSInput PSIn) : SV_Target {
    return g_Texture.Sample(g_Texture_sampler, PSIn.UV);
}
)";

struct PresentPass::Impl {
  RefCntAutoPtr<IPipelineState> pPSOLinear;
  RefCntAutoPtr<IPipelineState> pPSOPoint;
  RefCntAutoPtr<IShaderResourceBinding> pSRBLinear;
  RefCntAutoPtr<IShaderResourceBinding> pSRBPoint;
  RefCntAutoPtr<ISampler> pSamplerLinear;
  RefCntAutoPtr<ISampler> pSamplerPoint;

  ISwapChain *pSwapChain = nullptr;
};

PresentPass::PresentPass() : m_impl(new Impl()) {}

PresentPass::~PresentPass() {
  delete m_impl;
  m_impl = nullptr;
}

bool PresentPass::initialize(RenderDevice &device) {
  auto *pDevice = static_cast<IRenderDevice *>(device.getDevice());
  auto *pSwapChain = static_cast<ISwapChain *>(device.getSwapChain());

  if (!pDevice || !pSwapChain) {
    LOG_ERROR("PresentPass::initialize: Invalid device or swapchain");
    return false;
  }

  m_impl->pSwapChain = pSwapChain;
  const auto &SCDesc = pSwapChain->GetDesc();

  // Create vertex shader
  ShaderCreateInfo ShaderCI;
  ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  ShaderCI.Desc.UseCombinedTextureSamplers = true;

  RefCntAutoPtr<IShader> pVS;
  ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
  ShaderCI.EntryPoint = "main";
  ShaderCI.Desc.Name = "Present VS";
  ShaderCI.Source = VSSource;
  pDevice->CreateShader(ShaderCI, &pVS);

  if (!pVS) {
    LOG_ERROR("PresentPass: Failed to create vertex shader");
    return false;
  }

  // Create pixel shader
  RefCntAutoPtr<IShader> pPS;
  ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
  ShaderCI.Desc.Name = "Present PS";
  ShaderCI.Source = PSSource;
  pDevice->CreateShader(ShaderCI, &pPS);

  if (!pPS) {
    LOG_ERROR("PresentPass: Failed to create pixel shader");
    return false;
  }

  // Create linear sampler
  SamplerDesc SamLinearDesc;
  SamLinearDesc.MinFilter = FILTER_TYPE_LINEAR;
  SamLinearDesc.MagFilter = FILTER_TYPE_LINEAR;
  SamLinearDesc.MipFilter = FILTER_TYPE_LINEAR;
  SamLinearDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
  SamLinearDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
  SamLinearDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
  pDevice->CreateSampler(SamLinearDesc, &m_impl->pSamplerLinear);

  // Create point sampler (for integer_nearest per §5.8.3)
  SamplerDesc SamPointDesc;
  SamPointDesc.MinFilter = FILTER_TYPE_POINT;
  SamPointDesc.MagFilter = FILTER_TYPE_POINT;
  SamPointDesc.MipFilter = FILTER_TYPE_POINT;
  SamPointDesc.AddressU = TEXTURE_ADDRESS_CLAMP;
  SamPointDesc.AddressV = TEXTURE_ADDRESS_CLAMP;
  SamPointDesc.AddressW = TEXTURE_ADDRESS_CLAMP;
  pDevice->CreateSampler(SamPointDesc, &m_impl->pSamplerPoint);

  // Pipeline state descriptor
  GraphicsPipelineStateCreateInfo PSOCreateInfo;
  PSOCreateInfo.PSODesc.Name = "Present PSO";
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
  PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = SCDesc.ColorBufferFormat;
  PSOCreateInfo.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
  PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
      PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
  PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

  PSOCreateInfo.pVS = pVS;
  PSOCreateInfo.pPS = pPS;

  // Shader resource layout
  ShaderResourceVariableDesc Vars[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
  PSOCreateInfo.PSODesc.ResourceLayout.Variables = Vars;
  PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);

  // Create PSO with linear sampler
  ImmutableSamplerDesc ImtblSamplersLinear[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", SamLinearDesc}};
  PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplersLinear;
  PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
      _countof(ImtblSamplersLinear);
  PSOCreateInfo.PSODesc.Name = "Present PSO Linear";

  pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_impl->pPSOLinear);
  if (m_impl->pPSOLinear) {
    m_impl->pPSOLinear->CreateShaderResourceBinding(&m_impl->pSRBLinear, true);
  }

  // Create PSO with point sampler
  ImmutableSamplerDesc ImtblSamplersPoint[] = {
      {SHADER_TYPE_PIXEL, "g_Texture", SamPointDesc}};
  PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = ImtblSamplersPoint;
  PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers =
      _countof(ImtblSamplersPoint);
  PSOCreateInfo.PSODesc.Name = "Present PSO Point";

  pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_impl->pPSOPoint);
  if (m_impl->pPSOPoint) {
    m_impl->pPSOPoint->CreateShaderResourceBinding(&m_impl->pSRBPoint, true);
  }

  if (!m_impl->pPSOLinear || !m_impl->pPSOPoint) {
    LOG_ERROR("PresentPass: Failed to create pipeline states");
    return false;
  }

  LOG_INFO("PresentPass initialized");
  return true;
}

void PresentPass::execute(RenderDevice &device, void *cbufSRV, u32 cbufWidth,
                          u32 cbufHeight, PresentMode mode) {
  auto *pContext = static_cast<IDeviceContext *>(device.getContext());
  if (!pContext || !m_impl->pSwapChain || !cbufSRV)
    return;

  auto *pRTV = m_impl->pSwapChain->GetCurrentBackBufferRTV();
  const auto &SCDesc = m_impl->pSwapChain->GetDesc();

  // Calculate viewport
  Viewport vp = calculateViewport(
      static_cast<i32>(SCDesc.Width), static_cast<i32>(SCDesc.Height),
      static_cast<i32>(cbufWidth), static_cast<i32>(cbufHeight), mode);

  // Clear backbuffer with letterbox color (per §5.8.6)
  pContext->SetRenderTargets(1, &pRTV, nullptr,
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  pContext->ClearRenderTarget(pRTV, m_letterboxColor,
                              RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Select PSO based on mode (point for integer_nearest per §5.8.3)
  IPipelineState *pPSO = (mode == PresentMode::IntegerNearest)
                             ? m_impl->pPSOPoint.RawPtr()
                             : m_impl->pPSOLinear.RawPtr();
  IShaderResourceBinding *pSRB = (mode == PresentMode::IntegerNearest)
                                     ? m_impl->pSRBPoint.RawPtr()
                                     : m_impl->pSRBLinear.RawPtr();

  if (!pPSO || !pSRB)
    return;

  // Bind texture
  auto *pTextureSRV = static_cast<ITextureView *>(cbufSRV);
  pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pTextureSRV);

  pContext->SetPipelineState(pPSO);
  pContext->CommitShaderResources(pSRB,
                                  RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Set viewport
  Diligent::Viewport DiligentVP;
  DiligentVP.TopLeftX = static_cast<float>(vp.x);
  DiligentVP.TopLeftY = static_cast<float>(vp.y);
  DiligentVP.Width = static_cast<float>(vp.w);
  DiligentVP.Height = static_cast<float>(vp.h);
  DiligentVP.MinDepth = 0.0f;
  DiligentVP.MaxDepth = 1.0f;
  pContext->SetViewports(1, &DiligentVP, SCDesc.Width, SCDesc.Height);

  // Draw fullscreen triangle (3 vertices, no index buffer)
  DrawAttribs drawAttribs;
  drawAttribs.NumVertices = 3;
  pContext->Draw(drawAttribs);
}

void PresentPass::setLetterboxColor(f32 r, f32 g, f32 b, f32 a) {
  m_letterboxColor[0] = r;
  m_letterboxColor[1] = g;
  m_letterboxColor[2] = b;
  m_letterboxColor[3] = a;
}

} // namespace arcanee::render
