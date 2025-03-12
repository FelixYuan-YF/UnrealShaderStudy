// ExampleGraphicsShaderModule.h
#pragma once

#include "Modules/ModuleManager.h"

class FExampleGraphicsShaderModule : public IModuleInterface {
	virtual void StartupModule() override;
	// virtual void ShutdownModule() override;
};


// 这个struct对应hlsl里面的VertexAttributes
EXAMPLEGRAPHICSSHADER_API struct VertexAttributes
{
	FVector4f Position; // Normally we use homogeneous coordinate so we declared Vec4 but here for demonstration we only use first 2 components to store NDC
	FVector4f Color; // RGBA
};


// 存VS和PS对应的渲染资源
class EXAMPLEGRAPHICSSHADER_API FExampleGraphicsShaderResource : public FRenderResource
{
	static FExampleGraphicsShaderResource* GInstance; // Singleton instance
public:
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void ReleaseRHI() override;

	FVertexDeclarationRHIRef VertexDeclarationRHI; // 定义vertex的数据是如何存储在buffer的。见InitRHI()
	FReadBuffer VertexBuffer; // GPU只读不写，所以定义为ReadBuffer
	static FExampleGraphicsShaderResource* Get(); // Singleton instance
};

EXAMPLEGRAPHICSSHADER_API void RenderExampleGraphicsShader_GameThread(UTextureRenderTarget2D* TextureRenderTarget2D, FExampleGraphicsShaderResource* Resource);