// ExampleGraphicsShaderModule.cpp
#include "ExampleGraphicsShaderModule.h"

#include "ClearQuad.h"
#include "SelectionSet.h"
#include "Engine/TextureRenderTarget2D.h"

IMPLEMENT_MODULE(FExampleGraphicsShaderModule, ExampleGraphicsShader)

void FExampleGraphicsShaderModule::StartupModule()
{
	// 在模块初始化后将项目目录的Shaders文件夹Mount到/MyGraphicsShader
	AddShaderSourceDirectoryMapping(TEXT("/MyGraphicsShader"), FPaths::ProjectDir() / TEXT("Shaders"));
}

class FExampleGraphicsShaderVS: public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FExampleGraphicsShaderVS, EXAMPLEGRAPHICSSHADER_API);
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}
};

class FExampleGraphicsShaderPS: public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FExampleGraphicsShaderPS, EXAMPLEGRAPHICSSHADER_API);
	static bool ShouldCache(EShaderPlatform Platform)
	{
		return true;
	}
};


IMPLEMENT_SHADER_TYPE(, FExampleGraphicsShaderVS, TEXT("/MyGraphicsShader/ExampleGraphicsShader.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(, FExampleGraphicsShaderPS, TEXT("/MyGraphicsShader/ExampleGraphicsShader.usf"), TEXT("MainPS"), SF_Pixel);



FExampleGraphicsShaderResource*  FExampleGraphicsShaderResource::GInstance = nullptr;
FExampleGraphicsShaderResource* FExampleGraphicsShaderResource::Get()
{
	if(GInstance==nullptr)
	{
		GInstance = new FExampleGraphicsShaderResource();
		ENQUEUE_RENDER_COMMAND(FInitExampleGraphicsShaderResource)([](FRHICommandList& RHICmdList)
		{
			GInstance->InitResource(RHICmdList);
		});
	}
	return GInstance;
}

void FExampleGraphicsShaderResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	// 我们先hard code顶点数据
	TArray<VertexAttributes> Vertices;
	Vertices.Add({FVector4f(-1, 1, 1, 1), FVector4f(1,0,0,1)});
	Vertices.Add({FVector4f(1, 1,1, 1), FVector4f(0,0,1,1)});
	Vertices.Add({FVector4f(-1,-1,1, 1), FVector4f(1,1,0,1)});

	// 初始化buffer并拷贝
	uint32 NumBytes = sizeof(VertexAttributes)* Vertices.Num();
	FRHIResourceCreateInfo CreateInfo(TEXT("VertexBuffer"));
	VertexBuffer.Buffer = RHICmdList.CreateVertexBuffer(NumBytes, BUF_Static, CreateInfo);
	VertexAttributes* GPUBufferPtr = static_cast<VertexAttributes*>(RHICmdList.LockBuffer(VertexBuffer.Buffer, 0, sizeof(VertexAttributes) * Vertices.Num(), RLM_WriteOnly));
	FMemory::Memcpy(GPUBufferPtr, Vertices.GetData(), NumBytes);
	RHICmdList.UnlockBuffer(VertexBuffer.Buffer);
	// 到此为止，我们的buffer存储了3个vertices，每个vertex有4个float作为position和4个float作为color。所以一个vertex是4*sizeof(float) + 4*sizeof(float)==32字节。当然3个vertices总共96个字节。

	// 下面我们定义vertex buffer的数据排布。

	// 定义VertexDeclaration
	uint16 Stride = sizeof(VertexAttributes);
	FVertexDeclarationElementList Elements;
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(VertexAttributes, Position),VET_Float4,0, Stride));
	Elements.Add(FVertexElement(0, STRUCT_OFFSET(VertexAttributes, Color), VET_Float4, 1, Stride));
	VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	// 根据vertex的定义，前四个float是position，并且我们在hlsl将其定义为ATTRIBUTE0。所以它的InOffset应该是0（因为是结构体的第一个成员）；stride应该是sizeof(VertexAttributes)因为当前指针到下一个vertex指针的偏移量是sizeof(VertexAttributes)
	// Color同理，但它的InOffset是Position的大小，即4*sizeof(float)
	// 如果读者熟悉opengl API，FVertexElement与glVertexAttribPointer相似
	
}

void FExampleGraphicsShaderResource::ReleaseRHI()
{
	if(VertexBuffer.Buffer)
	{
		VertexBuffer.Release();
	}
	VertexDeclarationRHI.SafeRelease();
}

void RenderExampleGraphicsShader_RenderThread(FRHICommandList& RHICmdList, FExampleGraphicsShaderResource* Resource, FRHITexture* RenderTarget)
{
	// 调用shader进行渲染
	// 与compute shader不同，graphics shader需要我们定义：1）一个或多个Render Target；2）如何处理Rasterizer、Blend、Depth和Stencil的行为
	// 所以需要给到的初始化参数比compute shader多
	// 一个很好的例子是DrawClearQuad函数，里面详细写了从RHICmdList.BeginRenderPass到RHICmdList.EndRenderPass的所有步骤。如果你的shader无法工作，请使用DrawClearQuad函数进行测试。
	FRHIRenderPassInfo RPInfo(RenderTarget, ERenderTargetActions::Clear_Store);
	RHICmdList.BeginRenderPass(RPInfo, TEXT("ExampleGraphicsShaderRenderPass"));
	// 如果需要测试，可以在这里调用DrawClearQuad(RHICmdList, true, FLinearColor(FVector4(1, 0, 1, 1)), true, 1.0, true, 0);
	// 同时注释其他代码。
	auto ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);

	TShaderMapRef<FExampleGraphicsShaderVS> VertexShader(ShaderMap);
	TShaderMapRef<FExampleGraphicsShaderPS> PixelShader(ShaderMap);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<true, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = Resource->VertexDeclarationRHI;

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);
	RHICmdList.SetStreamSource(0, Resource->VertexBuffer.Buffer, 0);

	RHICmdList.DrawPrimitive(0, 1, 1);
	RHICmdList.EndRenderPass();
}

void RenderExampleGraphicsShader_GameThread(UTextureRenderTarget2D* TextureRenderTarget2D, FExampleGraphicsShaderResource* Resource)
{
	ENQUEUE_RENDER_COMMAND(FRenderExampleGraphicsShader)([Resource, TextureRenderTarget2D](FRHICommandList& RHICmdList)
	{
		RenderExampleGraphicsShader_RenderThread(RHICmdList, Resource, TextureRenderTarget2D->GetResource()->GetTexture2DRHI());
	});
}