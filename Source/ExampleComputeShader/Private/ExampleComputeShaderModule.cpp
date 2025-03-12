// ExampleComputeShaderModule.cpp
#include "ExampleComputeShaderModule.h"

IMPLEMENT_MODULE(FExampleComputeShaderModule, ExampleComputeShader)

void FExampleComputeShaderModule::StartupModule()
{
	// 在模块初始化后将项目目录的Shaders文件夹Mount到/MyShaders
	AddShaderSourceDirectoryMapping(TEXT("/MyShaders"), FPaths::ProjectDir() / TEXT("Shaders"));
}

// 添加头文件
#include "ShaderParameterStruct.h"

// 使用ShaderParameterStruct.h内的宏定义Shader参数结构体。
// **注意** 这里每个变量名字必须与usf文件中Shader参数名一致。
BEGIN_SHADER_PARAMETER_STRUCT(FExampleComputeShaderParameters,)
SHADER_PARAMETER(float, Scale)     // 基本类型的shader参数
SHADER_PARAMETER(float, Translate) // 基本类型的shader参数
SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, InputBuffer)  // UAV Buffer类型的shader参数，需要我们后续手动管理
SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutputBuffer) // UAV Buffer类型的shader参数，需要我们后续手动管理
END_SHADER_PARAMETER_STRUCT()


/**
 * FGlobalShader是定义在全局上的，Unreal也有FMaterialShader，目前我们不做讨论。
 * FGlobalShader的初始化将由Unreal定义的宏进行管理，我们可以使用宏方便地初始化Global Shader
 * 需要注意的是，FGlobalShader不能出现成员变量。我们会用其他的方法设置Shader参数
 */
class FExampleComputeShaderCS : public FGlobalShader
{
public:
	DECLARE_SHADER_TYPE(FExampleComputeShaderCS, Global) // 定义一堆函数，我们不用管是什么
	SHADER_USE_PARAMETER_STRUCT(FExampleComputeShaderCS, FGlobalShader)  // 定义该Shader使用SHADER_PARAMETER_STRUCT来定义Shader参数

	using FParameters = FExampleComputeShaderParameters; // SHADER_USE_PARAMETER_STRUCT需要我们定义FParameters，我们在类外定义的，所以只需要using就可以。
	//当然也可以直接在FGlobalShader内部定义BEGIN_SHADER_PARAMETER_STRUCT(FParameters,) 两种方法使用一个即可
};

IMPLEMENT_SHADER_TYPE(, FExampleComputeShaderCS, TEXT("/MyShaders/ExampleComputeShader.usf"), TEXT("FunctionMultiply"), SF_Compute)
// 使用宏自动实现FExampleComputeShaderCS的函数。第一个参数可为空，第二个参数是类名，第三个是usf的path，第四个是usf的入口函数名，第五个是shader类型(SF_Vertex, SF_Pixel, SF_Compute等)。

// ExampleComputeShaderModule.cpp

// 初始化单例指针
FExampleComputeShaderResource* FExampleComputeShaderResource::GInstance = nullptr;

FExampleComputeShaderResource* FExampleComputeShaderResource::Get()
{
	if(GInstance==nullptr)
	{
		GInstance = new FExampleComputeShaderResource();
		// 创建执行在RenderThread的任务。第一个括号写全局唯一的标识符，一般为这个Task起个名字。第二个写Lambda表达式，用FRHICommandList& RHICmdList作为函数参数。
		ENQUEUE_RENDER_COMMAND(FInitExampleComputeShaderResource)([](FRHICommandList& RHICmdList)
		{
			GInstance->InitResource(RHICmdList);
		});
	}
	return GInstance;
}

// Buffer初始化函数
void FExampleComputeShaderResource::InitRHI(FRHICommandListBase& RHICmdList)
{
	InputBuffer.Initialize(RHICmdList, TEXT("InputBuffer"), sizeof(float), 1);
	OutputBuffer.Initialize(RHICmdList, TEXT("OutputBuffer"), sizeof(float), 1);
}

// Buffer释放函数
void FExampleComputeShaderResource::ReleaseRHI()
{
	InputBuffer.Release();
	OutputBuffer.Release();
}

// ExampleComputeShaderModule.cpp
void DispatchExampleComputeShader_RenderThread(FRHICommandList& RHICmdList, FExampleComputeShaderResource* Resource, float Scale, float Translate, uint32 ThreadGroupX, uint32 ThreadGroupY, uint32 ThreadGroupZ)
{
	TShaderMapRef<FExampleComputeShaderCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel)); // 声明ShaderMap
	SetComputePipelineState(RHICmdList, Shader.GetComputeShader()); // 设置该RHICmdList的Pipeline为ComputeShader
	{
		typename FExampleComputeShaderCS::FParameters Parameters{}; // 创建Shader参数

		// 设置基本类型
		Parameters.Scale = Scale;
		Parameters.Translate = Translate;

		// 设置buffer类型
		Parameters.InputBuffer = Resource->InputBuffer.UAV;
		Parameters.OutputBuffer = Resource->OutputBuffer.UAV;

		// 传入参数
		SetShaderParameters(RHICmdList, Shader, Shader.GetComputeShader(), Parameters);
	}

	// 调用Compute shader
	DispatchComputeShader(RHICmdList, Shader.GetShader(), ThreadGroupX, ThreadGroupY, ThreadGroupZ);

	// 取消绑定buffer参数
	//UnsetShaderSRVs(RHICmdList, Shader, Shader.GetComputeShader());
	UnsetShaderUAVs(RHICmdList, Shader, Shader.GetComputeShader());
}

void DispatchExampleComputeShader_GameThread(float InputVal, float Scale, float Translate, FExampleComputeShaderResource* Resource)
{
	// 加入RenderThread任务
	ENQUEUE_RENDER_COMMAND(FDispatchExampleComputeShader)([Resource, InputVal, Scale, Translate](FRHICommandListImmediate& RHICmdList)
	{
		// LockBuffer并写入数据
		float* InputGPUBuffer = static_cast<float*>(RHICmdList.LockBuffer(Resource->InputBuffer.Buffer, 0, sizeof(float), RLM_WriteOnly));
		*InputGPUBuffer = InputVal; // 可以使用FMemory::Memcpy
		// UnlockBuffer
		RHICmdList.UnlockBuffer(Resource->InputBuffer.Buffer);
		// 调用RenderThread版本的函数
		DispatchExampleComputeShader_RenderThread(RHICmdList, Resource, Scale, Translate, 1, 1, 1);
	});
}

float GetGPUReadback(FExampleComputeShaderResource* Resource, float& OutputVal)
{
	float* pOutputVal = &OutputVal;
	// Flush所有RenderingCommands, 确保我们的shader已经执行了
	FlushRenderingCommands();
	ENQUEUE_RENDER_COMMAND(FReadbackOutputBuffer)([Resource, &pOutputVal](FRHICommandListImmediate& RHICmdList)
	{
		// LockBuffer并读取数据
		float* OutputGPUBuffer = static_cast<float*>(RHICmdList.LockBuffer(Resource->OutputBuffer.Buffer, 0, sizeof(float), RLM_ReadOnly));
		*pOutputVal = *OutputGPUBuffer;
	});
	// FlushRenderingCommands, 确保上面的RenderCommand被执行了
	FlushRenderingCommands();
	return OutputVal;

	// 下面是使用FRHIGPUBufferReadback的调用方式, 效果是相同的
	//FRHIGPUBufferReadback ReadbackBuffer(TEXT("ExampleComputeShaderReadback"));
	//FlushRenderingCommands();
	//ENQUEUE_RENDER_COMMAND(FEnqueueGPUReadback)([&ReadbackBuffer, Resource](FRHICommandListImmediate& RHICmdList)
	//{
	//	ReadbackBuffer.EnqueueCopy(RHICmdList, Resource->OutputBuffer.Buffer, sizeof(int32));
	//});
	//FlushRenderingCommands();
	//float OutputVal;
	//ENQUEUE_RENDER_COMMAND(FEnqueueGPUReadbackLock)([&ReadbackBuffer, Resource, &OutputVal](FRHICommandListImmediate& RHICmdList)
	//{
	//	float* pOutputVal = static_cast<float*>(ReadbackBuffer.Lock(sizeof(float)));
	//	checkf(pOutputVal!=nullptr, TEXT("ReadbackBuffer failed to lock"));
	//	OutputVal = *pOutputVal;
	//	ReadbackBuffer.Unlock();
	//});
	//FlushRenderingCommands();
	//return OutputVal;
}