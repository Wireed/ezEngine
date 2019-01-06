#include "Main.h"

#include <Core/Graphics/Geometry.h>
#include <Foundation/Time/Clock.h>
#include <Foundation/Utilities/Stats.h>
#include <GameEngine/GameApplication/WindowOutputTarget.h>
#include <RendererCore/RenderContext/RenderContext.h>
#include <RendererCore/ShaderCompiler/ShaderManager.h>
#include <RendererFoundation/Device/SwapChain.h>
#include <RendererFoundation/Resources/Texture.h>
#include <System/Window/Window.h>

static ezUInt32 g_uiWindowWidth = 1920;
static ezUInt32 g_uiWindowHeight = 1080;
static ezUInt32 g_uiComputeThreadGroupSize = 32;

ezComputeShaderHistogramApp::ezComputeShaderHistogramApp()
    : ezGameApplication("ComputeShaderHistogram", "../../..\\Data\\Samples\\ComputeShaderHistogram") //"ezEngine Project/ComputeShaderHistogram")
    , m_pWindow(nullptr)
{
}

ezComputeShaderHistogramApp::~ezComputeShaderHistogramApp() {}

ezApplication::ApplicationExecution ezComputeShaderHistogramApp::Run()
{
  ProcessWindowMessages();
  ezClock::GetGlobalClock()->Update();
  UpdateInput();

  m_stuffChanged = false;
  m_directoryWatcher->EnumerateChanges(ezMakeDelegate(&ezComputeShaderHistogramApp::OnFileChanged, this));
  if (m_stuffChanged)
  {
    ezResourceManager::ReloadAllResources(false);
  }

  // do the rendering
  {
    auto device = ezGALDevice::GetDefaultDevice();

    // Before starting to render in a frame call this function
    device->BeginFrame();

    // The ezGALContext class is the main interaction point for draw / compute operations
    ezGALContext& GALContext = *device->GetPrimaryContext();
    ezRenderContext& renderContext = *ezRenderContext::GetDefaultInstance();

    // Constant buffer update
    {
      auto& globalConstants = renderContext.WriteGlobalConstants();
      ezMemoryUtils::ZeroFill(&globalConstants);

      globalConstants.ViewportSize =
          ezVec4((float)g_uiWindowWidth, (float)g_uiWindowHeight, 1.0f / (float)g_uiWindowWidth, 1.0f / (float)g_uiWindowHeight);
      // Wrap around to prevent floating point issues. Wrap around is dividable by all whole numbers up to 11.
      globalConstants.GlobalTime = (float)ezMath::Mod(ezClock::GetGlobalClock()->GetAccumulatedTime().GetSeconds(), 20790.0);
      globalConstants.WorldTime = globalConstants.GlobalTime;
    }

    ezRectFloat viewport(0.0f, 0.0f, (float)g_uiWindowWidth, (float)g_uiWindowHeight);

    // Draw background.
    {
      ezGALRenderTagetSetup RTS;
      RTS.SetRenderTarget(0, m_hScreenRTV);
      renderContext.SetViewportAndRenderTargetSetup(viewport, RTS);

      renderContext.BindShader(m_hScreenShader);
      renderContext.BindMeshBuffer(ezGALBufferHandle(), ezGALBufferHandle(), nullptr, ezGALPrimitiveTopology::Triangles,
                                   1); // Vertices are generated by shader.
      renderContext.DrawMeshBuffer();
    }

    // Copy screentexture contents to backbuffer.
    // (Is drawing better? Don't care, this is a one liner and needs no shader!)
    {
      GALContext.CopyTexture(device->GetBackBufferTextureFromSwapChain(device->GetPrimarySwapChain()), m_hScreenTexture);
    }

    // Switch to backbuffer (so that the screen texture is no longer bound)
    {
      ezGALRenderTagetSetup RTS;
      RTS.SetRenderTarget(0, m_hBackbufferRTV);
      renderContext.SetViewportAndRenderTargetSetup(viewport, RTS);
    }

    // Compute histogram.
    {
      // Reset first.
      GALContext.ClearUnorderedAccessView(m_hHistogramUAV, ezVec4U32(0, 0, 0, 0));

      renderContext.BindShader(m_hHistogramComputeShader);
      renderContext.BindTexture2D("ScreenTexture", m_hScreenSRV);
      renderContext.BindUAV("HistogramOutput", m_hHistogramUAV);
      renderContext.Dispatch(g_uiWindowWidth / g_uiComputeThreadGroupSize + (g_uiWindowWidth % g_uiComputeThreadGroupSize != 0 ? 1 : 0),
                             g_uiWindowHeight / g_uiComputeThreadGroupSize + (g_uiWindowHeight % g_uiComputeThreadGroupSize != 0 ? 1 : 0));

      // Unbind UAV since it is used as SRV in next step. TODO: This should be handled automatically.
      renderContext.BindUAV("HistogramOutput", ezGALUnorderedAccessViewHandle());
      renderContext.ApplyContextStates();
    }

    // Draw histogram.
    {
      renderContext.BindShader(m_hHistogramDisplayShader);
      renderContext.BindMeshBuffer(m_hHistogramQuadMeshBuffer);
      renderContext.BindTexture2D("HistogramTexture", m_hHistogramSRV);
      renderContext.DrawMeshBuffer();

      // Unbind SRV since it is used as UAV in the next frame. TODO: This should be handled automatically.
      renderContext.BindTexture2D("HistogramTexture", ezGALResourceViewHandle());
      renderContext.ApplyContextStates();
    }

    device->Present(device->GetPrimarySwapChain(), true);

    device->EndFrame();
    ezRenderContext::GetDefaultInstance()->ResetContextState();
  }

  // needs to be called once per frame
  ezResourceManager::PerFrameUpdate();

  // tell the task system to finish its work for this frame
  // this has to be done at the very end, so that the task system will only use up the time that is left in this frame for
  // uploading GPU data etc.
  ezTaskSystem::FinishFrameTasks();

  return WasQuitRequested() ? ezApplication::ApplicationExecution::Quit : ezApplication::ApplicationExecution::Continue;
}

void ezComputeShaderHistogramApp::AfterCoreSystemsStartup()
{
  ezGameApplication::AfterCoreSystemsStartup();

  m_directoryWatcher = EZ_DEFAULT_NEW(ezDirectoryWatcher);
  EZ_VERIFY(m_directoryWatcher
                ->OpenDirectory(FindProjectDirectory(), ezDirectoryWatcher::Watch::Writes | ezDirectoryWatcher::Watch::Subdirectories)
                .Succeeded(),
            "Failed to watch project directory");

  EZ_VERIFY(ezPlugin::LoadPlugin("ezShaderCompilerHLSL").Succeeded(), "Compiler Plugin not found");

  auto device = ezGALDevice::GetDefaultDevice();

  // Create a window for rendering
  {
    m_pWindow = EZ_DEFAULT_NEW(ezWindow);
    ezWindowCreationDesc windowDesc;
    windowDesc.m_Resolution.width = g_uiWindowWidth;
    windowDesc.m_Resolution.height = g_uiWindowHeight;
    windowDesc.m_Title = "Compute Shader Histogram";
    m_pWindow->Initialize(windowDesc);
    ezWindowOutputTargetGAL* pOutputTarget = static_cast<ezWindowOutputTargetGAL*>(AddWindow(m_pWindow.Borrow()));
    device->SetPrimarySwapChain(pOutputTarget->m_hSwapChain);

    // Update window height/width constants with actual height/width.
    g_uiWindowHeight = m_pWindow->GetClientAreaSize().height;
    g_uiWindowWidth = m_pWindow->GetClientAreaSize().width;

    // Get backbuffer render target view.
    const ezGALSwapChain* pPrimarySwapChain = device->GetSwapChain(pOutputTarget->m_hSwapChain);
    m_hBackbufferRTV = device->GetDefaultRenderTargetView(pPrimarySwapChain->GetBackBufferTexture());
  }

  // Create textures and texture view for screen content (can't use backbuffer as shader resource view)
  {
    ezGALTextureCreationDescription texDesc;
    texDesc.m_uiWidth = g_uiWindowWidth;
    texDesc.m_uiHeight = g_uiWindowHeight;
    texDesc.m_Format = ezGALResourceFormat::RGBAUByteNormalized; // ezGALResourceFormat::RGBAUByteNormalizedsRGB;
    texDesc.m_bCreateRenderTarget = true;
    texDesc.m_bAllowShaderResourceView = true;

    m_hScreenTexture = device->CreateTexture(texDesc);
    m_hScreenRTV = device->GetDefaultRenderTargetView(m_hScreenTexture);
    m_hScreenSRV = device->GetDefaultResourceView(m_hScreenTexture);
  }

  // Create texture for histogram data.
  {
    ezGALTextureCreationDescription texDesc;
    texDesc.m_uiWidth = 256;
    texDesc.m_uiHeight = 3; // R, G, B
    texDesc.m_uiMipLevelCount = 1;
    texDesc.m_Format = ezGALResourceFormat::RUInt;
    texDesc.m_bCreateRenderTarget = false;
    texDesc.m_bAllowShaderResourceView = true;
    texDesc.m_bAllowUAV = true;
    texDesc.m_ResourceAccess.m_bImmutable = false;

    m_hHistogramTexture = device->CreateTexture(texDesc);
    m_hHistogramSRV = device->GetDefaultResourceView(m_hHistogramTexture);

    ezGALUnorderedAccessViewCreationDescription uavDesc;
    uavDesc.m_hTexture = m_hHistogramTexture;
    m_hHistogramUAV = device->CreateUnorderedAccessView(uavDesc);
  }

  // Setup Shaders and Materials
  {
    ezShaderManager::Configure("DX11_SM50", true);
    m_hScreenShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/screen.ezShader");
    m_hHistogramComputeShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/histogramcompute.ezShader");
    m_hHistogramDisplayShader = ezResourceManager::LoadResource<ezShaderResource>("Shaders/histogramdisplay.ezShader");
  }

  // Geometry.
  CreateHistogramQuad();
}

void ezComputeShaderHistogramApp::BeforeCoreSystemsShutdown()
{
  auto device = ezGALDevice::GetDefaultDevice();

  m_hScreenShader.Invalidate();
  m_hHistogramDisplayShader.Invalidate();
  m_hHistogramComputeShader.Invalidate();
  m_hHistogramQuadMeshBuffer.Invalidate();

  m_hScreenRTV.Invalidate();
  m_hScreenSRV.Invalidate();
  device->DestroyTexture(m_hScreenTexture);
  m_hScreenTexture.Invalidate();

  device->DestroyUnorderedAccessView(m_hHistogramUAV);
  m_hHistogramUAV.Invalidate();
  m_hHistogramSRV.Invalidate();
  device->DestroyTexture(m_hHistogramTexture);
  m_hHistogramTexture.Invalidate();

  ezGameApplication::BeforeCoreSystemsShutdown();
}

void ezComputeShaderHistogramApp::CreateHistogramQuad()
{
  m_hHistogramQuadMeshBuffer = ezResourceManager::GetExistingResource<ezMeshBufferResource>("{4BEFA142-FEDB-42D0-84DC-58223ADD8C62}");

  if (!m_hHistogramQuadMeshBuffer.IsValid())
  {
    ezVec2 pixToScreen(1.0f / g_uiWindowWidth * 0.5f, 1.0f / g_uiWindowHeight * 0.5f);
    const float borderOffsetPix = 80.0f;
    const float sizeScreen = 0.8f;

    ezGeometry geom;
    ezMat4 transform(ezMat3::IdentityMatrix(), ezVec3(1.0f - pixToScreen.x * borderOffsetPix - sizeScreen / 2,
                                                      -1.0f + pixToScreen.y * borderOffsetPix + sizeScreen / 2, 0.0f));
    geom.AddRectXY(ezVec2(sizeScreen, sizeScreen), ezColor::Black, transform);

    ezMeshBufferResourceDescriptor desc;
    desc.AddStream(ezGALVertexAttributeSemantic::Position, ezGALResourceFormat::XYZFloat);
    desc.AddStream(ezGALVertexAttributeSemantic::TexCoord0, ezGALResourceFormat::XYFloat);
    desc.AllocateStreams(geom.GetVertices().GetCount(), ezGALPrimitiveTopology::Triangles, geom.GetPolygons().GetCount() * 2);

    for (ezUInt32 v = 0; v < geom.GetVertices().GetCount(); ++v)
    {
      desc.SetVertexData<ezVec3>(0, v, geom.GetVertices()[v].m_vPosition);
    }
    // (Making use of knowledge of vertex order)
    desc.SetVertexData<ezVec2>(1, 0, ezVec2(0.0f, 0.0f));
    desc.SetVertexData<ezVec2>(1, 1, ezVec2(1.0f, 0.0f));
    desc.SetVertexData<ezVec2>(1, 2, ezVec2(1.0f, 1.0f));
    desc.SetVertexData<ezVec2>(1, 3, ezVec2(0.0f, 1.0f));

    ezUInt32 t = 0;
    for (ezUInt32 p = 0; p < geom.GetPolygons().GetCount(); ++p)
    {
      for (ezUInt32 v = 0; v < geom.GetPolygons()[p].m_Vertices.GetCount() - 2; ++v)
      {
        desc.SetTriangleIndices(t, geom.GetPolygons()[p].m_Vertices[0], geom.GetPolygons()[p].m_Vertices[v + 1],
                                geom.GetPolygons()[p].m_Vertices[v + 2]);

        ++t;
      }
    }

    m_hHistogramQuadMeshBuffer = ezResourceManager::CreateResource<ezMeshBufferResource>("{4BEFA142-FEDB-42D0-84DC-58223ADD8C62}", desc);
  }
}

void ezComputeShaderHistogramApp::OnFileChanged(const char* filename, ezDirectoryWatcherAction action)
{
  if (action == ezDirectoryWatcherAction::Modified)
  {
    ezLog::Info("The file {0} was modified", filename);
    m_stuffChanged = true;
  }
}

EZ_APPLICATION_ENTRY_POINT(ezComputeShaderHistogramApp);
