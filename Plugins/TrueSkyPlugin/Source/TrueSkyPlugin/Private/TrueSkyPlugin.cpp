// Copyright 2013-2015 Simul Software Ltd. All Rights Reserved.

#include "TrueSkyPluginPrivatePCH.h"

#include "TrueSkySequenceAsset.h"
#include "TrueSkySequenceActor.h"
#define INCLUDE_UE_EDITOR_FEATURES 0

#include "GenericWindow.h"

#include "RendererInterface.h"
#include "TrueSkyLightComponent.h"
#include "DynamicRHI.h"
#include "UnrealClient.h"
#include <map>
#include <algorithm>
#include <wchar.h>
#include "ModifyDefinitions.h"

#ifdef SIMUL_UE412_OLD_DEFS
typedef FPostOpaqueRenderParameters FRenderDelegateParameters;
typedef FPostOpaqueRenderDelegate FRenderDelegate;
#endif

#if PLATFORM_WINDOWS || PLATFORM_XBOXONE || PLATFORM_PS4
#define TRUESKY_PLATFORM_SUPPORTED 1
#else
#define TRUESKY_PLATFORM_SUPPORTED 0
#endif

#if PLATFORM_WINDOWS || PLATFORM_XBOXONE 
#define BREAK_IF_DEBUGGING if ( IsDebuggerPresent()) DebugBreak();
#else
#if PLATFORM_PS4
#define BREAK_IF_DEBUGGING SCE_BREAK();
#else
#define BREAK_IF_DEBUGGING
#endif
#endif
#define ERRNO_CHECK \
if (errno != 0)\
		{\
		BREAK_IF_DEBUGGING\
		errno = 0; \
		}

#if PLATFORM_PS4
#include <kernel.h>
#include <gnm.h>
#include <gnm/common.h>
#include <gnm/error.h>
#include <gnm/shader.h>
#include "GnmRHI.h"
#include "GnmRHIPrivate.h"

#include "GnmMemory.h"
#include "GnmContext.h"
#endif

// Dependencies.
#include "Core.h"
#include "RHI.h"
#include "GPUProfiler.h"
#include "ShaderCore.h"
#include "Engine.h"
#include "SceneUtils.h"	// For SCOPED_DRAW_EVENT
#include "StaticArray.h"
#include "ActorCrossThreadProperties.h"
#if WITH_EDITOR
#include "EditorViewportClient.h"
#endif
/** This is a macro that casts a dynamically bound RHI reference to the appropriate D3D type. */
#define DYNAMIC_CAST_D3D11RESOURCE(Type,Name) \
	FD3D11##Type* Name = (FD3D11##Type*)Name##RHI;

DEFINE_LOG_CATEGORY_STATIC(TrueSky, Log, All);

#if INCLUDE_UE_EDITOR_FEATURES
DECLARE_LOG_CATEGORY_EXTERN(LogD3D11RHI, Log, All);
#endif
#if PLATFORM_LINUX || PLATFORM_MAC
#include <wchar.h>
#endif
#if PLATFORM_MAC
#endif
#if PLATFORM_WINDOWS
#include "../Private/Windows/D3D11RHIBasePrivate.h"
// D3D RHI public headers.
#include "D3D11Util.h"
#include "D3D11State.h"
#include "D3D11Resources.h"
typedef unsigned __int64 uint64_t;
#endif

#if PLATFORM_XBOXONE
#include "../Private/XBoxOne/D3D11RHIBasePrivate.h"
// D3D RHI public headers.
#include "D3D11Util.h"
#include "D3D11State.h"
#include "D3D11Resources.h"
#endif
#include "Tickable.h"
//#include "TrueSkyPlugin.generated.inl"
#include "EngineModule.h"
#include <string>
#include <vector>

using namespace simul;

ActorCrossThreadProperties actorCrossThreadProperties;
ActorCrossThreadProperties *GetActorCrossThreadProperties()
{
	return &actorCrossThreadProperties;
}

void *GetPlatformDevice()
{
#if PLATFORM_PS4
	// PS4 has no concept of separate devices. For "device" we will specify the immediate context's BaseGfxContext.
	FRHICommandListImmediate& CommandList = FRHICommandListExecutor::GetImmediateCommandList();
	FGnmCommandListContext *ct=(FGnmCommandListContext*)(&CommandList.GetContext());
	sce::Gnmx::LightweightGfxContext *bs=&(ct->GetContext());
	//bs->validate();
	void * device =(void*)bs;
#else
	void * device =GDynamicRHI->RHIGetNativeDevice();
#endif
	return device;
}

void *GetPlatformContext(FRenderDelegateParameters& RenderParameters)
{
	void *pContext=nullptr;
#if PLATFORM_PS4
	FGnmCommandListContext *ctx=(FGnmCommandListContext*)(&RenderParameters.RHICmdList->GetContext());
	sce::Gnmx::LightweightGfxContext *bs=&(ctx->GetContext());
	pContext=bs;
#endif
	return pContext;
}

namespace simul
{
	namespace base
	{
		// Note: the following definition MUST exactly mirror the one in Simul/Base/FileLoader.h, but we wish to avoid includes from external projects,
		//  so it is reproduced here.
		class FileLoader
		{
		public:
			//! Returns a pointer to the current file handler.
			static FileLoader *GetFileLoader();
			//! Returns true if and only if the named file exists. If it has a relative path, it is relative to the current directory.
			virtual bool FileExists(const char *filename_utf8) const = 0;
			//! Set the file handling object: call this before any file operations, if at all.
			static void SetFileLoader(FileLoader *f);
			//! Put the file's entire contents into memory, by allocating sufficiently many bytes, and setting the pointer.
			//! The memory should later be freed by a call to \ref ReleaseFileContents.
			//! The filename should be unicode UTF8-encoded.
			virtual void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8, bool open_as_text) = 0;
			//! Get the file date as a julian day number. Return zero if the file doesn't exist.
			virtual double GetFileDate(const char* filename_utf8) = 0;
			//! Free the memory allocated by AcquireFileContents.		
			virtual void ReleaseFileContents(void* pointer) = 0;
			//! Save the chunk of memory to storage.
			virtual bool Save(void* pointer, unsigned int bytes, const char* filename_utf8, bool save_as_text) = 0;
		};
	}
	namespace unreal
	{
		class MemoryInterface
		{
		public:
			void* Allocate(size_t nbytes)
			{
				return AllocateTracked(nbytes, 1, nullptr);
			}
			void* Allocate(size_t nbytes, size_t align)
			{
				return AllocateTracked(nbytes, align, nullptr);
			}
			void* AllocateVideoMemory(size_t nbytes, size_t align)
			{
				return AllocateVideoMemoryTracked(nbytes, align, nullptr);
			}
			virtual void* AllocateTracked(size_t /*nbytes*/, size_t /*align*/, const char * /*fn_name*/){ return nullptr; }
			virtual void* AllocateVideoMemoryTracked(size_t /*nbytes*/, size_t /*align*/, const char * /*fn_name*/){ return nullptr; }
			virtual void Deallocate(void* address) = 0;
			virtual void DeallocateVideoMemory(void* address) = 0;

			virtual const char *GetNameAtIndex(int index) const=0;
			virtual int GetBytesAllocated(const char *name) const=0;
			virtual int GetTotalBytesAllocated() const=0;
			virtual int GetTotalVideoBytesAllocated() const=0;
		};
		class UE4FileLoader:public base::FileLoader
		{
			static FString ProcessFilename(const char *filename_utf8)
			{
				FString Filename(UTF8_TO_TCHAR(filename_utf8));
#if PLATFORM_PS4
				Filename = Filename.ToLower();
#endif
				Filename = Filename.Replace(UTF8_TO_TCHAR("\\"), UTF8_TO_TCHAR("/"));
				Filename = Filename.Replace(UTF8_TO_TCHAR("//"), UTF8_TO_TCHAR("/"));
				return Filename;
			}
		public:
			UE4FileLoader()
			{}
			~UE4FileLoader()
			{
			}
			bool FileExists(const char *filename_utf8) const
			{
				FString Filename = ProcessFilename(filename_utf8);
				bool result = FPlatformFileManager::Get().GetPlatformFile().FileExists(*Filename);
				// Now errno may be nonzero, which is unwanted.
				errno = 0;
				return result;
			}
			void AcquireFileContents(void*& pointer, unsigned int& bytes, const char* filename_utf8, bool open_as_text)
			{
				pointer = nullptr;
				bytes = 0;
				if (!FileExists(filename_utf8))
				{
					return;
				}
				FString Filename = ProcessFilename(filename_utf8);
				ERRNO_CHECK
				IFileHandle *fh = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Filename);
				if (!fh)
					return;
				ERRNO_CHECK
				bytes = fh->Size();
				// We append a zero, in case it's text. UE4 seems to not distinguish reading binaries or strings.
				pointer = new uint8[bytes+1];
				fh->Read((uint8*)pointer, bytes);
				((uint8*)pointer)[bytes] = 0;
				delete fh;
				ERRNO_CHECK
			}
			double GetFileDate(const char* filename_utf8)
			{
				FString Filename	=ProcessFilename(filename_utf8);
				FDateTime dt		=FPlatformFileManager::Get().GetPlatformFile().GetTimeStamp(*Filename);
				int64 uts			=dt.ToUnixTimestamp();
				double time_s		=( double(uts)/ 86400.0 );
				// Don't use FDateTime's GetModifiedJulianDay, it's only accurate to the nearest day!!
				return time_s;
			}
			void ReleaseFileContents(void* pointer)
			{
				delete [] ((uint8*)pointer);
			}
			bool Save(void* pointer, unsigned int bytes, const char* filename_utf8, bool save_as_text)
			{
				FString Filename = ProcessFilename(filename_utf8);
				IFileHandle *fh = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Filename);
				if (!fh)
				{
					errno=0;
					return false;
				}
				fh->Write((const uint8*)pointer, bytes);
				delete fh;
				errno=0;
				return true;
			}
		};
	}
}
using namespace simul;
using namespace unreal;

static void SetMultiResConstants(TArray<FVector2D> &MultiResConstants,FSceneView *View)
{
#ifdef NV_MULTIRES
	FMultiRes::RemapCBData RemapCBData;
	FMultiRes::CalculateRemapCBData(&View->MultiResConf, &View->MultiResViewports, &RemapCBData);
	
	 MultiResConstants.Add(RemapCBData.MultiResToLinearSplitsX);
	 MultiResConstants.Add(RemapCBData.MultiResToLinearSplitsY);
	
	for (int32 i = 0; i < 3; ++i)
	{
		MultiResConstants.Add(RemapCBData.MultiResToLinearX[i]);
    }
    for (int32 i = 0; i < 3; ++i)
    {
		MultiResConstants.Add(RemapCBData.MultiResToLinearY[i]);
    }
	MultiResConstants.Add(RemapCBData.LinearToMultiResSplitsX);
	MultiResConstants.Add(RemapCBData.LinearToMultiResSplitsY);
    for (int32 i = 0; i < 3; ++i)
    {
		MultiResConstants.Add(RemapCBData.LinearToMultiResX[i]);
    }
    for (int32 i = 0; i < 3; ++i)
    {
            MultiResConstants.Add(RemapCBData.LinearToMultiResY[i]);
    }
    check(FMultiRes::Viewports::Count == 9);
    for (int32 i = 0; i < FMultiRes::Viewports::Count; ++i)
    {
		const FViewportBounds& Viewport = View->MultiResViewportArray[i];
		MultiResConstants.Add(FVector2D(Viewport.TopLeftX, Viewport.TopLeftY));
		MultiResConstants.Add(FVector2D(Viewport.Width, Viewport.Height));
    }
    for (int32 i = 0; i < FMultiRes::Viewports::Count; ++i)
    {
		const FIntRect& ScissorRect = View->MultiResScissorArray[i];
		MultiResConstants.Add(FVector2D(ScissorRect.Min));
		MultiResConstants.Add(FVector2D(ScissorRect.Max));
    }
#endif
}

static simul::unreal::UE4FileLoader ue4SimulFileLoader;

#if PLATFORM_PS4
typedef SceKernelModule moduleHandle;
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Onion trueSKY"), STAT_Onion_trueSKY, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPUSystem, );
DECLARE_MEMORY_STAT_POOL_EXTERN(TEXT("Garlic trueSKY"), STAT_Garlic_trueSKY, STATGROUP_PS4RHI, FPlatformMemory::MCR_GPU, );
DEFINE_STAT(STAT_Onion_trueSKY);
DEFINE_STAT(STAT_Garlic_trueSKY);

class MemoryAllocator:public MemoryInterface
{
	std::map<void*,FMemBlock> memBlocks;
	std::map<std::string,int> memoryTracks;
	std::map<void *,std::string> allocationNames;
public:
	//! Allocate \a nbytes bytes of memory, aligned to \a align and return a pointer to them.
	virtual void* AllocateTracked(size_t nbytes,size_t align,const char *fn)
	{
		if(align==0)
			align=1;
		FMemBlock blck=FMemBlock::Allocate(nbytes,align,EGnmMemType::GnmMem_CPU,GET_STATID(STAT_Onion_trueSKY));
		void *ptr=blck.GetPointer();
		memBlocks[ptr]=blck;
		return ptr;
	}
	//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
	virtual void Deallocate(void* ptr)
	{
		if(ptr)
		{
			FMemBlock::Free(memBlocks[ptr]);
			memBlocks.erase(memBlocks.find(ptr));
		}
	}
	//! Allocate \a nbytes bytes of memory, aligned to \a align and return a pointer to them.
	virtual void* AllocateVideoMemoryTracked(size_t nbytes,size_t align,const char *fn)
	{
		if(align==0)
			align		=1;
		FMemBlock blck=FMemBlock::Allocate(nbytes,align,EGnmMemType::GnmMem_GPU,GET_STATID(STAT_Garlic_trueSKY));
		void *ptr=blck.GetPointer();
		memBlocks[ptr]=blck;
		if(fn&&strlen(fn)>0)
		{
			auto i=memoryTracks.find(fn);
			if(i==memoryTracks.end())
			{
				memoryTracks[fn]=0;
				i=memoryTracks.find(fn);
			}
			i->second+=nbytes;
			allocationNames[ptr]=fn;
		}
		return ptr;
	}
	//! De-allocate the memory at \param address (requires that this memory was allocated with Allocate()).
	virtual void DeallocateVideoMemory(void* ptr)
	{
		if(ptr)
		{
			auto m=memBlocks.find(ptr);
			if(m==memBlocks.end())
			{
				UE_LOG(TrueSky, Warning, TEXT("Trying to deallocate memory that's not been allocated: %d"),(int64)ptr);
				return;
			}
			int size=m->second.Size;
			FMemBlock::Free(m->second);
			memBlocks.erase(m);
			auto n=allocationNames.find(ptr);
			if(n!=allocationNames.end())
			{
				const std::string &name=n->second;
				if(name.length())
				{
					auto i=memoryTracks.find(name);
					if(i!=memoryTracks.end())
					{
						i->second-=size;
						if(i->second<=0)
							allocationNames.erase(n);
					}
				}
			}
		}
	}

	const char *GetNameAtIndex(int index) const override
	{
		std::map<std::string,int>::const_iterator i=memoryTracks.begin();
		for(int j=0;j<index&&i!=memoryTracks.end();j++)
		{
			i++;
		}
		if(i==memoryTracks.end())
			return nullptr;
		return i->first.c_str();
	}
	int GetBytesAllocated(const char *name) const override
	{
		if(!name)
			return 0;
		auto i=memoryTracks.find(name);
		if(i==memoryTracks.end())
			return 0;
		return i->second;
	}
	int GetTotalBytesAllocated() const
	{
		return 0;
	}
	virtual int GetTotalVideoBytesAllocated() const
	{
		int bytes=0;
		for(auto i:memoryTracks)
		{
			bytes+=i.second;
		}
		return bytes;
	}
};
MemoryAllocator ps4MemoryAllocator;
#else
typedef void* moduleHandle;
#endif


#if PLATFORM_WINDOWS || PLATFORM_XBOXONE

ID3D11Texture2D *GetPlatformTexturePtr(FRHITexture2D *t2)
{
	ID3D11Texture2D *T=nullptr;
	FD3D11TextureBase *m = static_cast<FD3D11Texture2D*>(t2);
	if(m)
	{
		T = (ID3D11Texture2D*)(m->GetResource());
	}
	return T;
}
ID3D11RenderTargetView *GetPlatformRenderTarget(FRHITexture2D *t2d)
{
	ID3D11RenderTargetView *T=nullptr;
	ID3D11RenderTargetView *rt=nullptr;
	FD3D11TextureBase *m = static_cast<FD3D11Texture2D*>(t2d);
	if(m)
	{
		T = (ID3D11RenderTargetView*)(m->GetRenderTargetView(0,0));
	}
	return T;
}

ID3D11Texture2D *GetPlatformTexturePtr(UTexture *t)
{
	if (t&&t->Resource)
	{
		FRHITexture2D *t2 = static_cast<FRHITexture2D*>(t->Resource->TextureRHI.GetReference());
		return GetPlatformTexturePtr(t2);
	}
	return nullptr;
}
ID3D11Texture2D *GetPlatformTexturePtr(FTexture *t)
{
	if (t)
	{
		FRHITexture2D *t2 = static_cast<FRHITexture2D*>(t->TextureRHI.GetReference());
		return GetPlatformTexturePtr(t2);
	}
	return nullptr;
}
#endif
#if PLATFORM_PS4
sce::Gnm::Texture *GetPlatformTexturePtr(FRHITexture2D *t2)
{
	sce::Gnm::Texture *T=nullptr;
	FGnmTexture2D *m = static_cast<FGnmTexture2D*>(t2);
	if(m)
	{
		T = (sce::Gnm::Texture*)(m->Surface.Texture);
	}
	return T;
}

sce::Gnm::Texture *GetPlatformTexturePtr(UTexture *t)
{
	if (t&&t->Resource)
	{
		FRHITexture2D *t2 = static_cast<FRHITexture2D*>(t->Resource->TextureRHI.GetReference());
		return GetPlatformTexturePtr(t2);
	}
	return nullptr;
}

sce::Gnm::Texture *GetPlatformTexturePtr(FTexture *t)
{
	if (t)
	{
		FRHITexture2D *t2 = static_cast<FRHITexture2D*>(t->TextureRHI.GetReference());
		return GetPlatformTexturePtr(t2);
	}
	return nullptr;
}

sce::Gnm::RenderTarget *GetPlatformRenderTarget(FRHITexture2D *t2d)
{
	sce::Gnm::RenderTarget *T=nullptr;
	FGnmTexture2D *m = static_cast<FGnmTexture2D*>(t2d);
	if(m)
	{
		T = (sce::Gnm::RenderTarget*)(m->Surface.ColorBuffer);
	}
	return T;
}

#endif

#ifndef UE_LOG_ONCE
#define UE_LOG_ONCE(a,b,c) {static bool done=false; if(!done) {UE_LOG(a,b,c, TEXT(""));done=true;}}
#endif

static void FlipFlapMatrix(FMatrix &v,bool flipx,bool flipy,bool flipz,bool flapx,bool flapy,bool flapz)
{
	if(flipx)
	{
		v.M[0][0]*=-1.f;
		v.M[0][1]*=-1.f;
		v.M[0][2]*=-1.f;
		v.M[0][3]*=-1.f;
	}
	if(flipy)
	{
		v.M[1][0]*=-1.f;
		v.M[1][1]*=-1.f;
		v.M[1][2]*=-1.f;
		v.M[1][3]*=-1.f;
	}
	if(flipz)
	{
		v.M[2][0]*=-1.f;
		v.M[2][1]*=-1.f;
		v.M[2][2]*=-1.f;
		v.M[2][3]*=-1.f;
	}
	if(flapx)
	{
		v.M[0][0]*=-1.f;
		v.M[1][0]*=-1.f;
		v.M[2][0]*=-1.f;
		v.M[3][0]*=-1.f;
	}
	if(flapy)
	{
		v.M[0][1]*=-1.f;
		v.M[1][1]*=-1.f;
		v.M[2][1]*=-1.f;
		v.M[3][1]*=-1.f;
	}
	if(flapz)
	{
		v.M[0][2]*=-1.f;
		v.M[1][2]*=-1.f;
		v.M[2][2]*=-1.f;
		v.M[3][2]*=-1.f;
	}
}

static void AdaptProjectionMatrix(FMatrix &projMatrix, float metresPerUnit)
{
	projMatrix.M[2][0]	*=-1.0f;
	projMatrix.M[2][1]	*=-1.0f;
	projMatrix.M[2][3]	*=-1.0f;
	projMatrix.M[3][2]	*= metresPerUnit;
}
// Bizarrely, Res = Mat1.operator*(Mat2) means Res = Mat2^T * Mat1, as
 //* opposed to Res = Mat1 * Mat2.
//  Equally strangely, the view matrix we get from rendering is NOT the same orientation as the one from the Editor Viewport class.

// NOTE: What Unreal calls "y", we call "x". This is because trueSKY uses the engineering standard of a right-handed coordinate system,
// Whereas UE uses the graphical left-handed coordinates.
//
void AdaptViewMatrix(FMatrix &viewMatrix,float metresPerUnit,const FMatrix &worldToSkyMatrix)
{
	FMatrix u=worldToSkyMatrix*viewMatrix;
	u.M[3][0]	*= metresPerUnit;
	u.M[3][1]	*= metresPerUnit;
	u.M[3][2]	*= metresPerUnit;
	static float U=0.f,V=90.f,W=0.f;
	static float Ue=0.f,Ve=0.f,We=0.f;
	FRotator rot(U,V,W);
	FMatrix v;
	{
		FRotationMatrix RotMatrix(rot);
		FMatrix r	=RotMatrix.GetTransposed();
		v			=r.operator*(u);
		static bool x=true,y=false,z=false,X=false,Y=false,Z=true;
		FlipFlapMatrix(v,x,y,z,X,Y,Z);
	}
	viewMatrix=v;
}

// Just an ordinary transformation matrix: we must convert it from UE's left-handed system to right-handed for trueSKY
static void RescaleMatrix(FMatrix &viewMatrix,float metresPerUnit)
{
	viewMatrix.M[3][0]	*= metresPerUnit;
	viewMatrix.M[3][1]	*= metresPerUnit;
	viewMatrix.M[3][2]	*= metresPerUnit;
	static bool fulladapt=true;
	if(fulladapt)
	{
		static float U=0.f,V=-90.f,W=0.f;
		FRotationMatrix RotMatrix(FRotator(U,V,W));
		FMatrix r=RotMatrix.GetTransposed();
		FMatrix v=viewMatrix.operator*(r);		// i.e. r * viewMatrix
		static bool postm=true;
		if(postm)
			v=RotMatrix*v;
		
	
		static bool x=true,y=false,z=false,X=true,Y=false,Z=false;
		FlipFlapMatrix(v,x,y,z,X,Y,Z);
		static bool inv=true;
		if(inv)
			v=v.Inverse();
		else
			v=v;
		viewMatrix=v;
		return;
	}
}
static void AdaptCubemapMatrix(FMatrix &viewMatrix)
{
	static float U=0.f,V=90.f,W=0.f;
	FRotationMatrix RotMatrix(FRotator(U,V,W));
	FMatrix r=RotMatrix.GetTransposed();
	FMatrix v=viewMatrix.operator*(r);
	static bool postm=true;
	if(postm)
		v=r.operator*(viewMatrix);
	{
		static bool x=true,y=false,z=false,X=false,Y=false,Z=true;
		FlipFlapMatrix(v,x,y,z,X,Y,Z);
	}
	{
		static bool x=true,y=true,z=false,X=false,Y=false,Z=false;
		FlipFlapMatrix(v,x,y,z,X,Y,Z);
	}
	viewMatrix=v;
}


#define ENABLE_AUTO_SAVING


static std::wstring Utf8ToWString(const char *src_utf8)
{
	int src_length=(int)strlen(src_utf8);
#ifdef _MSC_VER
	int length = MultiByteToWideChar(CP_UTF8, 0, src_utf8,src_length, 0, 0);
#else
	int length=src_length;
#endif
	wchar_t *output_buffer = new wchar_t [length+1];
#ifdef _MSC_VER
	MultiByteToWideChar(CP_UTF8, 0, src_utf8, src_length, output_buffer, length);
#else
	mbstowcs(output_buffer, src_utf8, (size_t)length );
#endif
	output_buffer[length]=0;
	std::wstring wstr=std::wstring(output_buffer);
	delete [] output_buffer;
	return wstr;
}
static std::string WStringToUtf8(const wchar_t *src_w)
{
	int src_length=(int)wcslen(src_w);
#ifdef _MSC_VER
	int size_needed = WideCharToMultiByte(CP_UTF8, 0,src_w, (int)src_length, nullptr, 0, nullptr, nullptr);
#else
	int size_needed=2*src_length;
#endif
	char *output_buffer = new char [size_needed+1];
#ifdef _MSC_VER
	WideCharToMultiByte (CP_UTF8,0,src_w,(int)src_length,output_buffer, size_needed, nullptr, nullptr);
#else
	wcstombs(output_buffer, src_w, (size_t)size_needed );
#endif
	output_buffer[size_needed]=0;
	std::string str_utf8=std::string(output_buffer);
	delete [] output_buffer;
	return str_utf8;
}
static std::string FStringToUtf8(const FString &Source)
{
	std::string str_utf8;
	const wchar_t *src_w=(const wchar_t*)(Source.GetCharArray().GetData());
	if(!src_w)
		return str_utf8;
	int src_length=(int)wcslen(src_w);
#ifdef _MSC_VER
	int size_needed = WideCharToMultiByte(CP_UTF8, 0,src_w, (int)src_length, nullptr, 0, nullptr, nullptr);
#else
	int size_needed=2*src_length;
#endif
	char *output_buffer = new char [size_needed+1];
#ifdef _MSC_VER
	WideCharToMultiByte (CP_UTF8,0,src_w,(int)src_length,output_buffer, size_needed, nullptr, nullptr);
#else
	wcstombs(output_buffer, src_w, (size_t)size_needed );
#endif
	output_buffer[size_needed]=0;
	str_utf8=std::string(output_buffer);
	delete [] output_buffer;
	return str_utf8;
}

static FString Utf8ToFString(const char *src_utf8)
{
	int src_length=(int)strlen(src_utf8);
#ifdef _MSC_VER
	int length = MultiByteToWideChar(CP_UTF8, 0, src_utf8,src_length, 0, 0);
#else
	int length=src_length;
#endif
	wchar_t *output_buffer = new wchar_t [length+1];
#ifdef _MSC_VER
	MultiByteToWideChar(CP_UTF8, 0, src_utf8, src_length, output_buffer, length);
#else
	mbstowcs(output_buffer, src_utf8, (size_t)length );
#endif
	output_buffer[length]=0;
	FString wstr=FString(output_buffer);
	delete [] output_buffer;
	return wstr;
}
#define DECLARE_TOGGLE(name)\
	void					OnToggle##name();\
	bool					IsToggled##name();

#define IMPLEMENT_TOGGLE(name)\
	void FTrueSkyPlugin::OnToggle##name()\
{\
	if(StaticGetRenderBool!=nullptr&&StaticSetRenderBool!=nullptr)\
	{\
		bool current=StaticGetRenderBool(#name);\
		StaticSetRenderBool(#name,!current);\
	}\
}\
\
bool FTrueSkyPlugin::IsToggled##name()\
{\
	if(StaticGetRenderBool!=nullptr)\
		if(StaticGetRenderBool)\
			return StaticGetRenderBool(#name);\
	return false;\
}

#define DECLARE_ACTION(name)\
	void					OnTrigger##name()\
	{\
		if(StaticTriggerAction!=nullptr)\
			StaticTriggerAction(#name);\
	}


class FTrueSkyTickable : public  FTickableGameObject
{
public:
	/** Tick interface */
	void					Tick( float DeltaTime );
	bool					IsTickable() const;
	TStatId					GetStatId() const;
};

struct Variant32
{
	union
	{
		float floatVal;
		int32 intVal;
	};
};

class FTrueSkyPlugin : public ITrueSkyPlugin
#ifdef SHARED_FROM_THIS
	, public TSharedFromThis<FTrueSkyPlugin,(ESPMode::Type)0>
#endif
{
	FCriticalSection criticalSection;
	bool GlobalOverlayFlag;
public:
	FTrueSkyPlugin();
	virtual ~FTrueSkyPlugin();

	static FTrueSkyPlugin*	Instance;
	void					OnDebugTrueSky(class UCanvas* Canvas, APlayerController*);

	/** IModuleInterface implementation */
	virtual void			StartupModule() override;
	virtual void			ShutdownModule() override;
	virtual bool			SupportsDynamicReloading() override;

	/** Render delegates */
	void					DelegatedRenderFrame( FRenderDelegateParameters& RenderParameters );
	void					DelegatedRenderPostTranslucent( FRenderDelegateParameters& RenderParameters );
	void					DelegatedRenderOverlays(FRenderDelegateParameters& RenderParameters);
	void					RenderFrame(uint64_t uid,FRenderDelegateParameters& RenderParameters);
	void					RenderPostTranslucent(uint64_t uid,FRenderDelegateParameters& RenderParameters);
	void					RenderOverlays(uint64_t uid,FRenderDelegateParameters& RenderParameters );
	
	// This updates the TrueSkyLightComponents i.e. trueSky Skylights
	void					UpdateTrueSkyLights(FRenderDelegateParameters& RenderParameters);


	/** Init rendering */
	bool					InitRenderingInterface(  );

	/** Enable rendering */
	void					SetRenderingEnabled( bool Enabled );

	/** If there is a TrueSkySequenceActor in the persistent level, this returns that actor's TrueSkySequenceAsset */
	UTrueSkySequenceAsset*	GetActiveSequence();
	void UpdateFromActor();
	
	void					*GetRenderEnvironment() override;
	bool					TriggerAction(const char *name);
	bool					TriggerAction(const FString &fname) override;
	static void				LogCallback(const char *txt);
	
	void					SetPointLight(int id,FLinearColor c,FVector pos,float min_radius,float max_radius) override;

	float					GetCloudinessAtPosition(int32 queryId,FVector pos) override;
	VolumeQueryResult		GetStateAtPosition(int32 queryId,FVector pos) override;
	float					CloudLineTest(int32 queryId,FVector StartPos,FVector EndPos) override;

	virtual void			SetRenderBool(const FString &fname, bool value) override;
	virtual bool			GetRenderBool(const FString &fname) const override;

	virtual void			SetRenderFloat(const FString &fname, float value) override;
	virtual float			GetRenderFloat(const FString &fname) const override;
	virtual float			GetRenderFloatAtPosition(const FString &fname,FVector pos) const override;
	
	void					SetRender(const FString &fname,const TArray<FVariant> &params) override;
	void					SetRenderInt(const FString& name, int value) override;
	int						GetRenderInt(const FString& name) const override;
	int						GetRenderInt(const FString& name,const TArray<FVariant> &params) const override;
	
	virtual void			SetRenderString(const FString &fname, const FString & value) override;
	virtual FString			GetRenderString(const FString &fname) const override;

	virtual void			SetKeyframeFloat(unsigned uid,const FString &fname, float value) override;
	virtual float			GetKeyframeFloat(unsigned uid,const FString &fname) const override;
							   
	void					SetKeyframeInt(unsigned uid,const FString& name, int value) override;
	int						GetKeyframeInt(unsigned uid,const FString& name) const override;
	
	virtual void			SetCloudShadowRenderTarget(FRenderTarget *t);

	
	FMatrix UEToTrueSkyMatrix(bool apply_scale=true) const;
	FMatrix TrueSkyToUEMatrix(bool apply_scale=true) const;
	virtual UTexture *GetAtmosphericsTexture()
	{
		return AtmosphericsTexture;
	}
	struct CloudVolume
	{
		FTransform transform;
		FVector extents;
	};
	virtual void ClearCloudVolumes()
	{
	criticalSection.Lock();
		cloudVolumes.clear();
	criticalSection.Unlock();
	}
	virtual void SetCloudVolume(int i,FTransform tr,FVector ext)
	{
	criticalSection.Lock();
		cloudVolumes[i].transform=tr;
		cloudVolumes[i].extents=ext;
	criticalSection.Unlock();
	}
	
	virtual int32 SpawnLightning(FVector startpos,FVector endpos,float magnitude,FVector colour) override
	{
		startpos=UEToTrueSkyPosition(actorCrossThreadProperties.Transform,actorCrossThreadProperties.MetresPerUnit,startpos);
		endpos=UEToTrueSkyPosition(actorCrossThreadProperties.Transform,actorCrossThreadProperties.MetresPerUnit,endpos);
		return StaticSpawnLightning(((const float*)(&startpos)),((const float*)(&endpos)) , magnitude,((const float*)(&colour)) );
	}
	virtual void RequestColourTable(unsigned uid,int x,int y,int z) override
	{
		if(colourTableRequests.find(uid)==colourTableRequests.end()||colourTableRequests[uid]==nullptr)
			colourTableRequests[uid]=new ColourTableRequest;

		ColourTableRequest *req=colourTableRequests[uid];
		
		req->uid=uid;
		req->x=x;
		req->y=y;
		req->z=z;
	}
	virtual const ColourTableRequest *GetColourTable(unsigned uid) override
	{
		auto it=colourTableRequests.find(uid);
		if(it==colourTableRequests.end())
			return nullptr;
		if(it->second->valid==false)
			return nullptr;
		return it->second;
	}
	virtual void ClearColourTableRequests() override
	{
		for(auto i:colourTableRequests)
		{
			delete [] i.second->data;
			delete i.second;
		}
		colourTableRequests.clear();
	}
	virtual SimulVersion GetVersion() const override
	{
		return simulVersion;
	}
protected:
	std::map<unsigned,ColourTableRequest*> colourTableRequests;
	std::map<int,CloudVolume> cloudVolumes;
	bool					EnsureRendererIsInitialized();
	
	UTexture				*AtmosphericsTexture;
	void					RenderCloudShadow();
	void					OnMainWindowClosed(const TSharedRef<SWindow>& Window);

	/** Called when Toggle rendering button is pressed */
	void					OnToggleRendering();

	
	void					OnUIChangedSequence();
	void					OnUIChangedTime(float t);

	void					ExportCloudLayer(const FString& filenameUtf8,int index);

	/** Called when the Actor is pointed to a different sequence.*/
	void					SequenceChanged();
	/** Returns true if Toggle rendering button should be enabled */
	bool					IsToggleRenderingEnabled();
	/** Returns true if Toggle rendering button should be checked */
	bool					IsToggleRenderingChecked();

	/** Called when the Toggle Fades Overlay button is pressed*/
	DECLARE_TOGGLE(ShowFades)
	DECLARE_TOGGLE(ShowCompositing)
	DECLARE_TOGGLE(Show3DCloudTextures)
	DECLARE_TOGGLE(Show2DCloudTextures)

	
	void ShowDocumentation()
	{
	//	FString DocumentationUrl = FDocumentationLink::ToUrl(Link);
		FString DocumentationUrl="https://docs.simul.co/unrealengine";
		FPlatformProcess::LaunchURL(*DocumentationUrl, nullptr, nullptr);
	}
	
	/** Adds a TrueSkySequenceActor to the current scene */
	void					OnAddSequence();
	void					OnSequenceDestroyed();
	/** Returns true if user can add a sequence actor */
	bool					IsAddSequenceEnabled();

	/** Initializes all necessary paths */
	void					InitPaths();
	
	struct int4
	{
		int x,y,z,w;
	};
	struct Viewport
	{
		int x,y,w,h;
		float znear,zfar;
	};
	
	void SetEditorCallback(FTrueSkyEditorCallback c) override
	{
		TrueSkyEditorCallback=c;
	}
	void InitializeDefaults(ATrueSkySequenceActor *a) override
	{
		a->initializeDefaults=true;
	}

	void AdaptViewMatrix(FMatrix &viewMatrix,bool editor_version=false);
	
	typedef int (*FStaticInitInterface)(  );
	typedef int (*FGetSimulVersion)(int *major,int *minor,int *build);
	typedef void(*FStaticSetMemoryInterface)(MemoryInterface *);
	typedef int (*FStaticShutDownInterface)(  );
	typedef void (*FLogCallback)(const char *);
	typedef void (*FStaticSetDebugOutputCallback)(FLogCallback);
	typedef void (*FStaticSetGraphicsDevice)(void* device, GraphicsDeviceType deviceType,GraphicsEventType eventType);
	typedef int (*FStaticPushPath)(const char*,const char*);
	typedef void(*FStaticSetFileLoader)(simul::base::FileLoader *);
	typedef int (*FStaticGetOrAddView)( void *);
	typedef int (*FStaticGetLightningBolts)(LightningBolt *,int maxb);
	typedef int (*FStaticSpawnLightning)(const float startpos[3],const float endpos[3],float magnitude,const float colour[3]);
	typedef int (*FStaticRenderFrame)(void* device,void* deviceContext,int view_id
		,float* viewMatrix4x4
		,float* projMatrix4x4
		,void* depthTexture
		,void* depthResource
		,int4 depthViewport
		,const Viewport *v
		,PluginStyle s
		,float exposure
		,float gamma
		,int framenumber
		,const float *nvMultiResConstants);
	typedef int (*FStaticCopySkylight)(void *pContext
												,int cube_id
												,float* shValues
												,int shOrder
												,void *targetTex
												,const float *engineToSimulMatrix4x4);
	typedef int (*FStaticRenderOverlays)(void* device,void* deviceContext,void* depthTexture,const float* viewMatrix4x4,const float* projMatrix4x4,int view_id);
	typedef int (*FStaticTick)( float deltaTime );
	typedef int (*FStaticOnDeviceChanged)( void * device );
	typedef void* (*FStaticGetEnvironment)();
	typedef int (*FStaticSetSequence)( std::string sequenceInputText );
	typedef class UE4PluginRenderInterface* (*FStaticGetRenderInterfaceInstance)();
	typedef void (*FStaticSetPointLight)(int id,const float pos[3],float radius,float maxradius,const float radiant_flux[3]);
	typedef void (*FStaticCloudPointQuery)(int id,const float *pos, VolumeQueryResult *res);
	typedef void (*FStaticCloudLineQuery)(int id,const float *startpos,const float *endpos, LineQueryResult *res);
	
	typedef void(*FStaticSetRenderTexture)(const char *name, void* texturePtr);
	typedef void(*FStaticSetMatrix4x4)(const char *name, const float []);
	
	typedef void (*FStaticSetRender)( const char *name,int num_params,const Variant32 *params);
	typedef void (*FStaticSetRenderBool)( const char *name,bool value );
	typedef bool (*FStaticGetRenderBool)( const char *name );
	typedef void (*FStaticSetRenderFloat)( const char *name,float value );
	typedef float (*FStaticGetRenderFloat)( const char *name );
	
	typedef void (*FStaticSetRenderInt)( const char *name,int value );
	typedef int (*FStaticGetRenderInt)( const char *name,int num_params,const Variant32 *params);

	typedef void (*FStaticSetRenderString)( const char *name,const FString &value );
	typedef void (*FStaticGetRenderString)( const char *name ,char* value,int len);
	typedef bool (*FStaticTriggerAction)( const char *name );
	
	
	typedef void (*FStaticExportCloudLayerToGeometry)(const char *filenameUtf8,int index);

	typedef void (*FStaticSetKeyframeFloat)(unsigned,const char *name, float value);
	typedef float (*FStaticGetKeyframeFloat)(unsigned,const char *name);
	typedef void (*FStaticSetKeyframeInt)(unsigned,const char *name, int value);
	typedef int (*FStaticGetKeyframeInt)(unsigned,const char *name);

	typedef float (*FStaticGetRenderFloatAtPosition)(const char* name,const float *pos);

	typedef bool (*FStaticFillColourTable)(unsigned uid,int x,int y,int z,float *target);

	FTrueSkyEditorCallback				TrueSkyEditorCallback;

	FStaticInitInterface				StaticInitInterface;
	FGetSimulVersion					GetSimulVersion;
	FStaticSetMemoryInterface			StaticSetMemoryInterface;
	FStaticShutDownInterface			StaticShutDownInterface;
	FStaticSetDebugOutputCallback		StaticSetDebugOutputCallback;
	FStaticSetGraphicsDevice			StaticSetGraphicsDevice;
	FStaticPushPath						StaticPushPath;
	FStaticSetFileLoader				StaticSetFileLoader;
	FStaticGetOrAddView					StaticGetOrAddView;
	FStaticRenderFrame					StaticRenderFrame;
	FStaticCopySkylight					StaticCopySkylight;
						
	FStaticRenderOverlays				StaticRenderOverlays;
	FStaticTick							StaticTick;
	FStaticOnDeviceChanged				StaticOnDeviceChanged;
	FStaticGetEnvironment				StaticGetEnvironment;
	FStaticSetSequence					StaticSetSequence;
	FStaticGetRenderInterfaceInstance	StaticGetRenderInterfaceInstance;
	FStaticSetPointLight				StaticSetPointLight;
	FStaticCloudPointQuery				StaticCloudPointQuery;
	FStaticCloudLineQuery				StaticCloudLineQuery;
	FStaticSetRenderTexture				StaticSetRenderTexture;
	FStaticSetMatrix4x4					StaticSetMatrix4x4;
	FStaticSetRenderBool				StaticSetRenderBool;
	FStaticSetRender					StaticSetRender;
	FStaticGetRenderBool				StaticGetRenderBool;
	FStaticSetRenderFloat				StaticSetRenderFloat;
	FStaticGetRenderFloat				StaticGetRenderFloat;
	FStaticSetRenderInt					StaticSetRenderInt;
	FStaticGetRenderInt					StaticGetRenderInt;
	FStaticSetRenderString				StaticSetRenderString;
	FStaticGetRenderString				StaticGetRenderString;
	FStaticTriggerAction				StaticTriggerAction;
	
	FStaticExportCloudLayerToGeometry	StaticExportCloudLayerToGeometry;

	FStaticSetKeyframeFloat				StaticSetKeyframeFloat;
	FStaticGetKeyframeFloat				StaticGetKeyframeFloat;
	FStaticSetKeyframeInt				StaticSetKeyframeInt;
	FStaticGetKeyframeInt				StaticGetKeyframeInt;
	FStaticGetRenderFloatAtPosition		StaticGetRenderFloatAtPosition;
	FStaticGetLightningBolts			StaticGetLightningBolts;

	FStaticSpawnLightning				StaticSpawnLightning;

	FStaticFillColourTable				StaticFillColourTable;

	const TCHAR*			PathEnv;

	bool					RenderingEnabled;
	bool					RendererInitialized;

	float					CachedDeltaSeconds;
	float					AutoSaveTimer;		// 0.0f == no auto-saving
	
	FRenderTarget			*cloudShadowRenderTarget;

	bool					actorPropertiesChanged;
	bool					haveEditor;
	bool					exportNext;
	char					exportFilenameUtf8[100];
	UTrueSkySequenceAsset *sequenceInUse;
	int LastFrameNumber;
	
	SimulVersion simulVersion;
};

IMPLEMENT_MODULE( FTrueSkyPlugin, TrueSkyPlugin )


FTrueSkyPlugin* FTrueSkyPlugin::Instance = nullptr;

static FString trueSkyPluginPath="../../Plugins/TrueSkyPlugin";

FTrueSkyPlugin::FTrueSkyPlugin()
	:AtmosphericsTexture(nullptr)
	,cloudShadowRenderTarget(nullptr)
	,actorPropertiesChanged(true)
	,exportNext(false)
	,GlobalOverlayFlag(true)
	,sequenceInUse(nullptr)
	,LastFrameNumber(-1)
	,PathEnv(nullptr)
{
	simulVersion=ToSimulVersion(0,0,0);
	if(Instance)
		UE_LOG(TrueSky, Warning, TEXT("Instance is already set!"));
	
	InitPaths();
	Instance = this;
#ifdef SHARED_FROM_THIS
TSharedRef< FTrueSkyPlugin,(ESPMode::Type)0 > ref=AsShared();
#endif
#if PLATFORM_WINDOWS
	GlobalOverlayFlag=true;
#endif
	AutoSaveTimer = 0.0f;
	//we need to pass through real DeltaSecond; from our scene Actor?
	CachedDeltaSeconds = 0.0333f;

}

FTrueSkyPlugin::~FTrueSkyPlugin()
{
	Instance = nullptr;
}

bool FTrueSkyPlugin::SupportsDynamicReloading()
{
	return true;
}
void FTrueSkyPlugin::SetCloudShadowRenderTarget(FRenderTarget *t)
{
	cloudShadowRenderTarget=t;
}

void FTrueSkyPlugin::RenderCloudShadow()
{
	if(!cloudShadowRenderTarget)
		return;
//	FTextureRenderTarget2DResource* res = (FTextureRenderTarget2DResource*)cloudShadowRenderTarget->Resource;
/*	FCanvas* Canvas = new FCanvas(cloudShadowRenderTarget, nullptr, nullptr);
	Canvas->Clear(FLinearColor::Blue);
	// Write text (no text is visible since the Canvas has no effect
	UFont* Font = GEngine->GetSmallFont();
	Canvas->DrawShadowedString(100, 100, TEXT("Test"), Font, FLinearColor::White);
	Canvas->Flush();
	delete Canvas;*/
}

void *FTrueSkyPlugin::GetRenderEnvironment()
{
	if( StaticGetEnvironment != nullptr )
	{
		return StaticGetEnvironment();
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to call StaticGetEnvironment before it has been set"));
		return nullptr;
	}
}

void FTrueSkyPlugin::LogCallback(const char *txt)
{
	if(!Instance )
		return;
	static FString fstr;
	fstr+=txt;
	int max_len=0;
	for(int i=0;i<fstr.Len();i++)
	{
		if(fstr[i]==L'\n'||i>1000)
		{
			fstr[i]=L' ';
			max_len=i+1;
			break;
		}
	}
	if(max_len==0)
		return;
	FString substr=fstr.Left(max_len);
	fstr=fstr.RightChop(max_len);
	if(substr.Contains("error"))
	{
		UE_LOG(TrueSky,Error,TEXT("%s"), *substr);
	}
	else if(substr.Contains("warning"))
	{
		UE_LOG(TrueSky,Warning,TEXT("%s"), *substr);
	}
	else
	{
		UE_LOG(TrueSky,Display,TEXT("%s"), *substr);
	}
}

void FTrueSkyPlugin::AdaptViewMatrix(FMatrix &viewMatrix,bool editor_version)
{
	::AdaptViewMatrix(viewMatrix,actorCrossThreadProperties.MetresPerUnit,actorCrossThreadProperties.Transform.ToMatrixWithScale());
}

bool FTrueSkyPlugin::TriggerAction(const char *name)
{
	if( StaticTriggerAction != nullptr )
	{
		return StaticTriggerAction( name );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to TriggerAction before it has been set"));
	}
	return false;
}

bool FTrueSkyPlugin::TriggerAction(const FString &fname)
{
	std::string name=FStringToUtf8(fname);
	if( StaticTriggerAction != nullptr )
	{
		return StaticTriggerAction( name.c_str() );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to TriggerAction before it has been set"));
	}
	return false;
}
	
void FTrueSkyPlugin::SetRenderBool(const FString &fname, bool value)
{
	std::string name=FStringToUtf8(fname);
	if( StaticSetRenderBool != nullptr )
	{
		StaticSetRenderBool(name.c_str(), value );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set render bool before StaticSetRenderBool has been set"));
	}
}

bool FTrueSkyPlugin::GetRenderBool(const FString &fname) const
{
	std::string name=FStringToUtf8(fname);
	if( StaticGetRenderBool != nullptr )
	{
		return StaticGetRenderBool( name.c_str() );
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get render bool before StaticGetRenderBool has been set"));
	return false;
}

void FTrueSkyPlugin::SetRenderFloat(const FString &fname, float value)
{
	std::string name=FStringToUtf8(fname);
	if( StaticSetRenderFloat != nullptr )
	{
		StaticSetRenderFloat( name.c_str(), value );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set render float before StaticSetRenderFloat has been set"));
	}
}

float FTrueSkyPlugin::GetRenderFloat(const FString &fname) const
{
	std::string name=FStringToUtf8(fname);
	if( StaticGetRenderFloat != nullptr )
	{
		return StaticGetRenderFloat( name.c_str() );
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get render float before StaticGetRenderFloat has been set"));
	return 0.0f;
}

	

void FTrueSkyPlugin::SetRenderInt(const FString &fname, int value)
{
	std::string name=FStringToUtf8(fname);
	if( StaticSetRenderInt != nullptr )
	{
		StaticSetRenderInt( name.c_str(), value );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set render int before StaticSetRenderInt has been set"));
	}
}

	

void FTrueSkyPlugin::SetRender(const FString &fname, const TArray<FVariant> &params)
{
	std::string name=FStringToUtf8(fname);
	if( StaticSetRender != nullptr )
	{
		Variant32 varlist[6];
		int num_params=params.Num();
		if(num_params>5)
		{
			UE_LOG_ONCE(TrueSky, Warning, TEXT("Too many parameters."));
			return ;
		}
		for(int i=0;i<num_params;i++)
		{
			if(params[i].GetType()==EVariantTypes::Int32)
				varlist[i].intVal=params[i].GetValue<int32>();
			else if(params[i].GetType()==EVariantTypes::Int32)
				varlist[i].intVal=params[i].GetValue<int32>();
			else if(params[i].GetType()==EVariantTypes::Float)
				varlist[i].floatVal=params[i].GetValue<float>();
			else if(params[i].GetType()==EVariantTypes::Double)
				varlist[i].floatVal=params[i].GetValue<double>();
			else
			{
				UE_LOG_ONCE(TrueSky, Warning, TEXT("Unsupported variant type."));
				return ;
			}
		}
		varlist[num_params].intVal=0;
		StaticSetRender( name.c_str(), num_params,varlist);
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set render int before StaticSetRenderInt has been set"));
	}
}
int FTrueSkyPlugin::GetRenderInt(const FString &fname) const
{
	TArray<FVariant> params;
	return GetRenderInt(fname,params);
}

int FTrueSkyPlugin::GetRenderInt(const FString &fname,const TArray<FVariant> &params) const
{
	std::string name=FStringToUtf8(fname);
	if( StaticGetRenderInt != nullptr )
	{
		Variant32 varlist[6];
		int num_params=params.Num();
		if(num_params>5)
		{
			UE_LOG_ONCE(TrueSky, Warning, TEXT("Too many parameters."));
			return 0;
		}
		for(int i=0;i<num_params;i++)
		{
			if(params[i].GetType()==EVariantTypes::Int32)
				varlist[i].intVal=params[i].GetValue<int32>();
			else if(params[i].GetType()==EVariantTypes::Int32)
				varlist[i].intVal=params[i].GetValue<int32>();
			else if(params[i].GetType()==EVariantTypes::Float)
				varlist[i].floatVal=params[i].GetValue<float>();
			else if(params[i].GetType()==EVariantTypes::Double)
				varlist[i].floatVal=params[i].GetValue<double>();
			else
			{
				UE_LOG_ONCE(TrueSky, Warning, TEXT("Unsupported variant type."));
				return 0;
			}
		}
		varlist[num_params].intVal=0;
		return StaticGetRenderInt( name.c_str(),num_params,varlist);
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get render float before StaticGetRenderInt has been set"));
	return 0;
}

void FTrueSkyPlugin::SetRenderString(const FString &fname, const FString &value)
{
	if( StaticSetRenderString != nullptr )
	{
		std::string name=FStringToUtf8(fname);
		StaticSetRenderString( name.c_str(), value );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set render string before StaticSetRenderString has been set"));
	}
}

FString FTrueSkyPlugin::GetRenderString(const FString &fname) const
{
	if( StaticGetRenderString != nullptr )
	{
		std::string name=FStringToUtf8(fname);
		static char txt[4500];
		StaticGetRenderString( name.c_str(),txt,4500);
		return FString(txt);
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get render string before StaticGetRenderString has been set"));
	return "";
}





void FTrueSkyPlugin::SetKeyframeFloat(unsigned uid,const FString &fname, float value)
{
	if( StaticSetKeyframeFloat != nullptr )
	{
		std::string name=FStringToUtf8(fname);
		StaticSetKeyframeFloat(uid,name.c_str(), value );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set Keyframe float before StaticSetKeyframeFloat has been set"));
	}
}

float FTrueSkyPlugin::GetKeyframeFloat(unsigned uid,const FString &fname) const
{
	if( StaticGetKeyframeFloat != nullptr )
	{
	std::string name=FStringToUtf8(fname);
		return StaticGetKeyframeFloat( uid,name.c_str() );
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get Keyframe float before StaticGetKeyframeFloat has been set"));
	return 0.0f;
}

float FTrueSkyPlugin::GetRenderFloatAtPosition(const FString &fname,FVector pos) const
{
	if( StaticGetRenderFloatAtPosition != nullptr )
	{
		std::string name=FStringToUtf8(fname);
		ActorCrossThreadProperties *A=GetActorCrossThreadProperties();
		pos=UEToTrueSkyPosition(A->Transform,A->MetresPerUnit,pos);
		return StaticGetRenderFloatAtPosition( name.c_str(),(const float*)&pos);
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get float at position before StaticGetRenderFloatAtPosition has been set"));
	return 0.0f;
}
	

void FTrueSkyPlugin::SetKeyframeInt(unsigned uid,const FString &fname, int value)
{
	if( StaticSetKeyframeInt != nullptr )
	{
	std::string name=FStringToUtf8(fname);
		StaticSetKeyframeInt( uid,name.c_str(), value );
	}
	else
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to set Keyframe int before StaticSetKeyframeInt has been set"));
	}
}

int FTrueSkyPlugin::GetKeyframeInt(unsigned uid,const FString &fname) const
{
	if( StaticGetKeyframeInt != nullptr )
	{
	std::string name=FStringToUtf8(fname);
		return StaticGetKeyframeInt( uid,name.c_str() );
	}

	UE_LOG_ONCE(TrueSky, Warning, TEXT("Trying to get Keyframe float before StaticGetKeyframeInt has been set"));
	return 0;
}

/** Tickable object interface */
void FTrueSkyTickable::Tick( float DeltaTime )
{
	//if(FTrueSkyPlugin::Instance)
	//	FTrueSkyPlugin::Instance->Tick(DeltaTime);
}

bool FTrueSkyTickable::IsTickable() const
{
	return true;
}

TStatId FTrueSkyTickable::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTrueSkyTickable, STATGROUP_Tickables);
}

void FTrueSkyPlugin::StartupModule()
{
	if(FModuleManager::Get().IsModuleLoaded("MainFrame") )
		haveEditor=true;
	GetRendererModule().RegisterPostOpaqueRenderDelegate( FRenderDelegate::CreateRaw(this, &FTrueSkyPlugin::DelegatedRenderFrame) );
#ifndef SIMUL_UE412_OLD_DEFS
	GetRendererModule().RegisterPostTranslucentRenderDelegate( FRenderDelegate::CreateRaw(this, &FTrueSkyPlugin::DelegatedRenderPostTranslucent) );
#endif
	GetRendererModule().RegisterOverlayRenderDelegate( FRenderDelegate::CreateRaw(this, &FTrueSkyPlugin::DelegatedRenderOverlays) );
	
	RenderingEnabled					=false;
	RendererInitialized					=false;
	TrueSkyEditorCallback				=nullptr;
	StaticInitInterface					=nullptr;
	GetSimulVersion						=nullptr;
	StaticSetMemoryInterface			=nullptr;
	StaticShutDownInterface				=nullptr;
	StaticSetDebugOutputCallback		=nullptr;
	StaticSetGraphicsDevice				=nullptr;
	StaticPushPath						=nullptr;
	StaticSetFileLoader					=nullptr;
	StaticGetOrAddView					=nullptr;
	StaticRenderFrame					=nullptr;
	StaticCopySkylight					=nullptr;
	StaticRenderOverlays				=nullptr;
	StaticTick							=nullptr;
	StaticOnDeviceChanged				=nullptr;
	StaticGetEnvironment				=nullptr;
	StaticSetSequence					=nullptr;

	StaticGetRenderInterfaceInstance	=nullptr;
	StaticSetPointLight					=nullptr;
	StaticCloudPointQuery				=nullptr;
	StaticCloudLineQuery				=nullptr;
	StaticSetRenderTexture				=nullptr;
	StaticSetMatrix4x4					=nullptr;
	StaticSetRenderBool					=nullptr;
	StaticSetRender						=nullptr;
	StaticGetRenderBool					=nullptr;
	StaticTriggerAction					=nullptr;

	StaticSetRenderFloat				=nullptr;
	StaticGetRenderFloat				=nullptr;
	StaticSetRenderInt					=nullptr;
	StaticGetRenderInt					=nullptr;
	
	StaticSetRenderString				=nullptr;
	StaticGetRenderString				=nullptr;
										
	StaticExportCloudLayerToGeometry	=nullptr;
										
	StaticSetKeyframeFloat				=nullptr;
	StaticGetKeyframeFloat				=nullptr;
	StaticSetKeyframeInt				=nullptr;
	StaticGetKeyframeInt				=nullptr;
	StaticGetLightningBolts				=nullptr;
	StaticSpawnLightning				=nullptr;

	StaticGetRenderFloatAtPosition		=nullptr;
	StaticFillColourTable				=nullptr;
	PathEnv = nullptr;
}
//FGraphEventRef tsFence;
#if PLATFORM_PS4
#endif

void FTrueSkyPlugin::SetRenderingEnabled( bool Enabled )
{
	RenderingEnabled = Enabled;
}

struct FRHIRenderOverlaysCommand : public FRHICommand<FRHIRenderOverlaysCommand>
{
	FRenderDelegateParameters RenderParameters;
	FTrueSkyPlugin *TrueSkyPlugin;
	uint64_t uid;
	FORCEINLINE_DEBUGGABLE FRHIRenderOverlaysCommand(FRenderDelegateParameters p,
												FTrueSkyPlugin *d,uint64_t u)
												:RenderParameters(p)
												,TrueSkyPlugin(d)
												,uid(u)
	{
	}
    void Execute(FRHICommandListBase& CmdList)
	{
		TrueSkyPlugin->RenderOverlays(uid,RenderParameters );
	}
};

void FTrueSkyPlugin::DelegatedRenderOverlays(FRenderDelegateParameters& RenderParameters)
{	
	FSceneView *View=(FSceneView*)(RenderParameters.Uid);
	Viewport v;
	v.x=RenderParameters.ViewportRect.Min.X;
	v.y=RenderParameters.ViewportRect.Min.Y;
	v.w=RenderParameters.ViewportRect.Width();
	v.h=RenderParameters.ViewportRect.Height();
	v.znear = 0.0f;
	v.zfar = 1.0f;
	// We really don't want to be drawing on preview views etc. so set an arbitrary minimum view size.
	static int min_view_size=600;
	if(v.w<min_view_size&&v.h<min_view_size)
		return;
	uint64_t uid=(uint64_t)((unsigned)v.w<<(unsigned)24)+((unsigned)v.h<<(unsigned)16)+((unsigned)View->StereoPass);
	if (RenderParameters.RHICmdList->Bypass())
	{
		FRHIRenderOverlaysCommand Command(RenderParameters,this,uid);
		Command.Execute(*RenderParameters.RHICmdList);			
		return;
	}	
	new (RenderParameters.RHICmdList->AllocCommand<FRHIRenderOverlaysCommand>()) FRHIRenderOverlaysCommand(RenderParameters,this,uid);
}

void FTrueSkyPlugin::RenderOverlays(uint64_t uid,FRenderDelegateParameters& RenderParameters)
{	
#if PLATFORM_XBOXONE
	return;
#endif
#if TRUESKY_PLATFORM_SUPPORTED
	if(!GlobalOverlayFlag)
		return;
	if(!RenderParameters.ViewportRect.Width()||!RenderParameters.ViewportRect.Height())
		return;
	UpdateFromActor();
	if(!RenderingEnabled )
		return;
	if(!RendererInitialized )
		return;
	if( RenderingEnabled )
	{
		//StaticTick( 0 );

#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
		FD3D11DynamicRHI *d3dRHI				=(FD3D11DynamicRHI*)(GDynamicRHI);
		ID3D11DeviceContext *pContext			=nullptr;
		FD3D11TextureBase * depthTex			=static_cast<FD3D11Texture2D*>(RenderParameters.DepthTexture);	
		ID3D11Texture2D *depthTex_or_colourRT	=static_cast<ID3D11Texture2D*>(depthTex->GetResource());
#endif
		void *device=GetPlatformDevice();
#if PLATFORM_PS4
		void * depthTexture					=GetPlatformTexturePtr(RenderParameters.DepthTexture);
		void *depthTex_or_colourRT			=depthTexture;//GetPlatformRenderTarget(RenderParameters.RenderTargetTexture);
#endif
#if PLATFORM_PS4
		FGnmCommandListContext *ctx=(FGnmCommandListContext*)(&RenderParameters.RHICmdList->GetContext());
		sce::Gnmx::LightweightGfxContext *lwgfxc=&(ctx->GetContext());
		void *pContext=lwgfxc;
#endif
		Viewport v;
		v.x=RenderParameters.ViewportRect.Min.X;
		v.y=RenderParameters.ViewportRect.Min.Y;
		v.w=RenderParameters.ViewportRect.Width();
		v.h=RenderParameters.ViewportRect.Height();
        int view_id = StaticGetOrAddView((void*)uid);		// RVK: really need a unique view ident to pass here..
		static int overlay_id=0;
	//	if(overlay_id==view_id)
		{
			FMatrix projMatrix = RenderParameters.ProjMatrix;
			projMatrix.M[2][3]	*=-1.0f;
			projMatrix.M[3][2]	*= actorCrossThreadProperties.MetresPerUnit;
			FMatrix viewMatrix=RenderParameters.ViewMatrix;
			// We want the transform FROM worldspace TO trueskyspace
			AdaptViewMatrix(viewMatrix);
			StaticRenderOverlays(device,pContext,depthTex_or_colourRT,&(viewMatrix.M[0][0]),&(projMatrix.M[0][0]),view_id);
		}
#if PLATFORM_PS4
	ctx->RestoreCachedDCBState();
#endif
	}
#endif
}

struct FRHIPostOpaqueCommand: public FRHICommand<FRHIPostOpaqueCommand>
{
	FRenderDelegateParameters RenderParameters;
	FTrueSkyPlugin *TrueSkyPlugin;
	uint64_t uid;
	FORCEINLINE_DEBUGGABLE FRHIPostOpaqueCommand(FRenderDelegateParameters p,
		FTrueSkyPlugin *d,uint64_t u)
		:RenderParameters(p)
		,TrueSkyPlugin(d)
		,uid(u)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		if(TrueSkyPlugin)
		{
			TrueSkyPlugin->RenderFrame(uid,RenderParameters);
#ifdef SIMUL_UE412_OLD_DEFS
			TrueSkyPlugin->RenderPostTranslucent(uid,RenderParameters);
#endif
		}
	}
};

struct FRHIPostTranslucentCommand: public FRHICommand<FRHIPostTranslucentCommand>
{
	FRenderDelegateParameters RenderParameters;
	FTrueSkyPlugin *TrueSkyPlugin;
	uint64_t uid;
	FORCEINLINE_DEBUGGABLE FRHIPostTranslucentCommand(FRenderDelegateParameters p,
		FTrueSkyPlugin *d,uint64_t u)
		:RenderParameters(p)
		,TrueSkyPlugin(d)
		,uid(u)
	{
	}
	void Execute(FRHICommandListBase& CmdList)
	{
		if(TrueSkyPlugin)
			TrueSkyPlugin->RenderPostTranslucent(uid,RenderParameters);
	}
};

void FTrueSkyPlugin::DelegatedRenderPostTranslucent(FRenderDelegateParameters& RenderParameters)
{
#if PLATFORM_WINDOWS 
	SCOPED_DRAW_EVENT(*RenderParameters.RHICmdList, TrueSky);	//breaks on Xbox One 
#endif
	QUICK_SCOPE_CYCLE_COUNTER(TrueSky);
	FSceneView *View=(FSceneView*)(RenderParameters.Uid);
	Viewport v;
	v.x=RenderParameters.ViewportRect.Min.X;
	v.y=RenderParameters.ViewportRect.Min.Y;
	v.w=RenderParameters.ViewportRect.Width();
	v.h=RenderParameters.ViewportRect.Height();	// We really don't want to be drawing on preview views etc. so set an arbitrary minimum view size.
	static int min_view_size=257;
	if(v.w<min_view_size&&v.h<min_view_size)
		return;
	uint64_t uid=(uint64_t)((unsigned)v.w<<(unsigned)24)+((unsigned)v.h<<(unsigned)16)+((unsigned)View->StereoPass);
	if(RenderParameters.RHICmdList->Bypass())
	{
		FRHIPostTranslucentCommand Command(RenderParameters,this,uid);
		Command.Execute(*RenderParameters.RHICmdList);
		return;
	}
	new (RenderParameters.RHICmdList->AllocCommand<FRHIPostTranslucentCommand>()) FRHIPostTranslucentCommand(RenderParameters,this,uid);
}

void FTrueSkyPlugin::RenderPostTranslucent(uint64_t uid,FRenderDelegateParameters& RenderParameters)
{
#if TRUESKY_PLATFORM_SUPPORTED
	if(RenderParameters.Uid==0)
	{
		if(StaticShutDownInterface)
			StaticShutDownInterface();
		sequenceInUse=nullptr;
		return;
	}
	//check(IsInRenderingThread());
	if(!RenderParameters.ViewportRect.Width()||!RenderParameters.ViewportRect.Height())
		return;
	if(!EnsureRendererIsInitialized())
		return;
	if(!RenderingEnabled)
		return;
	if(!StaticGetOrAddView)
		return;
	QUICK_SCOPE_CYCLE_COUNTER(TrueSkyPostTrans);
	void *device=GetPlatformDevice();
	void *pContext=GetPlatformContext(RenderParameters);
	PluginStyle style=UNREAL_STYLE;
	FSceneView *View=(FSceneView*)(RenderParameters.Uid);
	if(View->StereoPass==eSSP_LEFT_EYE)
		uid=29435;
	else if(View->StereoPass==eSSP_RIGHT_EYE)
		uid=29435;
    int view_id = StaticGetOrAddView((void*)uid);		// RVK: really need a unique view ident to pass here..
			FMatrix viewMatrix=RenderParameters.ViewMatrix;
			// We want the transform FROM worldspace TO trueskyspace
			AdaptViewMatrix(viewMatrix);
	FMatrix projMatrix = RenderParameters.ProjMatrix;
	AdaptProjectionMatrix(projMatrix, actorCrossThreadProperties.MetresPerUnit);
	float exposure=actorCrossThreadProperties.Brightness;
	static float g=1.0f;
	float gamma=g;//actorCrossThreadProperties.Gamma;
	void * depthTexture					=GetPlatformTexturePtr(RenderParameters.DepthTexture);
	void *colourTarget=nullptr;
#if PLATFORM_PS4
	colourTarget						=GetPlatformRenderTarget(RenderParameters.RenderTargetTexture);
#endif
	Viewport v;
	int4 depthViewport;
	v.x=depthViewport.x=RenderParameters.ViewportRect.Min.X;
	v.y=depthViewport.y=RenderParameters.ViewportRect.Min.Y;
	v.w=depthViewport.z=RenderParameters.ViewportRect.Width();
	v.h=depthViewport.w=RenderParameters.ViewportRect.Height();
	
	 // NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
	TArray<FVector2D>  MultiResConstants;
#ifdef NV_MULTIRES
	bool bMultiResEnabled = View->bMultiResEnabled;
	if (bMultiResEnabled)
	{
		SetMultiResConstants(MultiResConstants,View);
	}
#else
	bool bMultiResEnabled = false;
#endif
	style=style|POST_TRANSLUCENT;
	StaticRenderFrame(device,pContext,view_id, &(viewMatrix.M[0][0]), &(projMatrix.M[0][0])
			,depthTexture,colourTarget
			,depthViewport
			,&v
			,style
			,exposure
			,gamma
			,GFrameNumber
           // NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
           ,(bMultiResEnabled ? (const float*)( MultiResConstants.GetData()) : nullptr)
           // NVCHANGE_END: TrueSky + VR MultiRes Support
			);
#endif
}

void FTrueSkyPlugin::DelegatedRenderFrame(FRenderDelegateParameters& RenderParameters)
{
#if PLATFORM_WINDOWS 
	SCOPED_DRAW_EVENT(*RenderParameters.RHICmdList, TrueSky);	//breaks on Xbox One 
#endif
	QUICK_SCOPE_CYCLE_COUNTER(TrueSky);
	FSceneView *View=(FSceneView*)(RenderParameters.Uid);
	Viewport v;
	v.x=RenderParameters.ViewportRect.Min.X;
	v.y=RenderParameters.ViewportRect.Min.Y;
	v.w=RenderParameters.ViewportRect.Width();
	v.h=RenderParameters.ViewportRect.Height();
	uint64_t uid=(uint64_t)((unsigned)v.w<<(unsigned)24)+((unsigned)v.h<<(unsigned)16)+((unsigned)View->StereoPass);
	if(RenderParameters.RHICmdList->Bypass())
	{
		FRHIPostOpaqueCommand Command(RenderParameters,this,uid);
		Command.Execute(*RenderParameters.RHICmdList);
		return;
	}
	new (RenderParameters.RHICmdList->AllocCommand<FRHIPostOpaqueCommand>()) FRHIPostOpaqueCommand(RenderParameters,this,uid);
}

void FTrueSkyPlugin::UpdateTrueSkyLights(FRenderDelegateParameters& RenderParameters)
{
	static FVector ue_pos(0.0f,1.0f,2.0f),ts_pos,ue_pos2;
	ts_pos=UEToTrueSkyPosition(actorCrossThreadProperties.Transform,actorCrossThreadProperties.MetresPerUnit,ue_pos);
	ue_pos2=TrueSkyToUEPosition(actorCrossThreadProperties.Transform,actorCrossThreadProperties.MetresPerUnit,ts_pos);
	TArray<UTrueSkyLightComponent*> &components=UTrueSkyLightComponent::GetTrueSkyLightComponents();
	int u=0;
	FMatrix transf=UEToTrueSkyMatrix(false);//.GetTransposed();
	//for (TObjectIterator<UTrueSkyLightComponent> It; It; ++It)
	for(int i=0;i<components.Num();i++)
	{
		UTrueSkyLightComponent* trueSkyLightComponent = components[i];//*It;
		FSkyLightSceneProxy *renderProxy=trueSkyLightComponent->GetSkyLightSceneProxy();
		if(renderProxy&&StaticCopySkylight)
		{
			void *pContext=GetPlatformContext(RenderParameters);
			const int nums=renderProxy->IrradianceEnvironmentMap.R.MaxSHBasis;
			int NumTotalFloats=renderProxy->IrradianceEnvironmentMap.R.NumSIMDVectors*renderProxy->IrradianceEnvironmentMap.R.NumComponentsPerSIMDVector;
			float shValues[nums*3];// 3 colours.
			//const int32 EffectiveTopMipSize = renderProxy->ProcessedTexture->GetSizeX();
			//const int32 NumMips = FMath::CeilLogTwo(EffectiveTopMipSize) + 1;

			// returns 0 if successful:
			if(!StaticCopySkylight(pContext
									,u
									,shValues
									,renderProxy->IrradianceEnvironmentMap.R.MaxSHOrder
									,GetPlatformTexturePtr(renderProxy->ProcessedTexture)
									,&(transf.M[0][0])))
			{
				// There are two things to do here. First we must fill in the output texture.
				// second, we must put numbers in the spherical harmonics vector.
				static float m=0.05f;
				float mix=m;
				static int maxnum=9;
				if(!trueSkyLightComponent->IsInitialized())
				{
					for(int i=0;i<nums;i++)
					{
						renderProxy->IrradianceEnvironmentMap.R.V[i]=0.0f;
						renderProxy->IrradianceEnvironmentMap.G.V[i]=0.0f;
						renderProxy->IrradianceEnvironmentMap.B.V[i]=0.0f;
					}
				}
				for(int i=0;i<nums;i++)
				{
					renderProxy->IrradianceEnvironmentMap.R.V[i]*=(1.0f-mix);
					renderProxy->IrradianceEnvironmentMap.G.V[i]*=(1.0f-mix);
					renderProxy->IrradianceEnvironmentMap.B.V[i]*=(1.0f-mix);
					renderProxy->IrradianceEnvironmentMap.R.V[i]+=mix*shValues[i*3];
					renderProxy->IrradianceEnvironmentMap.G.V[i]+=mix*shValues[i*3+1];
					renderProxy->IrradianceEnvironmentMap.B.V[i]+=mix*shValues[i*3+2];
				}
				for(int i=maxnum;i<NumTotalFloats;i++)
				{
					renderProxy->IrradianceEnvironmentMap.R.V[i]=0.0f;
					renderProxy->IrradianceEnvironmentMap.G.V[i]=0.0f;
					renderProxy->IrradianceEnvironmentMap.B.V[i]=0.0f;
				}
				trueSkyLightComponent->SetInitialized(true);
			}
		}
		u++;
	}
}

void FTrueSkyPlugin::RenderFrame(uint64_t uid,FRenderDelegateParameters& RenderParameters)
{
#if TRUESKY_PLATFORM_SUPPORTED
	if(RenderParameters.Uid==0)
	{
		if(StaticShutDownInterface)
			StaticShutDownInterface();
		sequenceInUse=nullptr;
		return;
	}
	//check(IsInRenderingThread());
	if(!RenderParameters.ViewportRect.Width()||!RenderParameters.ViewportRect.Height())
		return;
	if(!EnsureRendererIsInitialized())
		return;
	UpdateFromActor();
	if(!RenderingEnabled )
		return;
	FSceneView *View=(FSceneView*)(RenderParameters.Uid);
	
	Viewport v;
	v.x=RenderParameters.ViewportRect.Min.X;
	v.y=RenderParameters.ViewportRect.Min.Y;
	v.w=RenderParameters.ViewportRect.Width();
	v.h=RenderParameters.ViewportRect.Height();
	
	StaticTick(0);
	FMatrix viewMatrix					 = RenderParameters.ViewMatrix;
	void * depthTexture					=GetPlatformTexturePtr(RenderParameters.DepthTexture);
#if PLATFORM_WINDOWS || PLATFORM_XBOXONE
	void *colourTarget=nullptr;
#endif
		
#if PLATFORM_PS4
	void *colourTarget					=GetPlatformRenderTarget(RenderParameters.RenderTargetTexture);
#endif
	void *rain_cubemap_dx11				=GetPlatformTexturePtr(actorCrossThreadProperties.RainCubemap);
	StaticSetRenderTexture("Cubemap", rain_cubemap_dx11);

	void *moon_texture_dx11				=GetPlatformTexturePtr(actorCrossThreadProperties.MoonTexture);
	void *cosmic_texture_dx11			=GetPlatformTexturePtr(actorCrossThreadProperties.CosmicBackgroundTexture);
	//FD3D11TextureBase *cubemapTex		=static_cast<FD3D11Texture2D*>(RenderParameters.CubemapTexture);
	//void *cubemap_texture_dx11			=GetPlatformTexturePtr(RenderParameters.CubemapTexture);//?(ID3D11Texture2D*)cubemapTex->GetResource():nullptr;
	StaticSetRenderTexture("Moon"		,moon_texture_dx11);
	StaticSetRenderTexture("Background"	,cosmic_texture_dx11);
	void *loss_texture_dx11				=GetPlatformTexturePtr(actorCrossThreadProperties.LossRT);
	void *insc_texture_dx11				=GetPlatformTexturePtr(actorCrossThreadProperties.InscatterRT);
	void *cloud_vis_texture				=GetPlatformTexturePtr(actorCrossThreadProperties.CloudVisibilityRT);
	StaticSetRenderTexture("Loss2D"		,loss_texture_dx11);
	StaticSetRenderTexture("Inscatter2D",insc_texture_dx11);
	StaticSetRenderTexture("CloudVisibilityRT", cloud_vis_texture);
	if(simulVersion>=SIMUL_4_1)
	{
		void *cloud_shadow_texture			=GetPlatformTexturePtr(actorCrossThreadProperties.CloudShadowRT);
		StaticSetRenderTexture("CloudShadowRT", cloud_shadow_texture);
	}
	//StaticSetRenderTexture("Cubemap"	, cubemap_texture_dx11);
	// if running or simulating, we want to update the "real time" value for trueSKY:
	//StaticSetRenderFloat("realTime"		, actorCrossThreadProperties.Time);
	StaticSetRenderFloat("sky:MaxSunRadiance"		, actorCrossThreadProperties.MaxSunRadiance);
	
	// Apply the sub-component volumes:
	//TArray< UActorComponent * > comps=GetComponentsByClass(    UCumulonimbusComponent::GetClass());
	criticalSection.Lock();
	for(auto i=cloudVolumes.begin();i!=cloudVolumes.end();i++)
	{
		Variant32 params[20];
		params[0].intVal=i->first;
		FMatrix m=i->second.transform.ToMatrixWithScale();
		// We want the transform FROM worldspace TO volumespace
		//m=m.Inverse();
		RescaleMatrix(m,actorCrossThreadProperties.MetresPerUnit);
		for(int j=0;j<16;j++)
		{
			params[j+1].floatVal=((const float*)m.M)[j];
		}
		FVector ext=i->second.extents*actorCrossThreadProperties.MetresPerUnit;// We want this in metres.
		for(int j=0;j<3;j++)
		{
			params[17+j].floatVal=ext[j];
		}
		StaticSetRender("CloudVolume",20,params);
	}
	criticalSection.Unlock();
	FMatrix cubemapMatrix;
	cubemapMatrix.SetIdentity();
	AdaptCubemapMatrix(cubemapMatrix);
	StaticSetMatrix4x4("CubemapTransform", &(cubemapMatrix.M[0][0]));
	float exposure=actorCrossThreadProperties.Brightness;
	static float g=1.0f;
	float gamma=g;//actorCrossThreadProperties.Gamma;

	// unreal unit is 10cm???
	FMatrix projMatrix = RenderParameters.ProjMatrix;
	AdaptProjectionMatrix(projMatrix, actorCrossThreadProperties.MetresPerUnit);
	//projMatrix.M[2][0]	*=-1.0f;
	//projMatrix.M[2][1]	*=-1.0f;
	//projMatrix.M[2][3]	*=-1.0f;
	//projMatrix.M[3][2]	*= actorCrossThreadProperties.MetresPerUnit;
	void *device=GetPlatformDevice();
	void *pContext=GetPlatformContext(RenderParameters);
	AdaptViewMatrix(viewMatrix);
	PluginStyle style=UNREAL_STYLE;
	
	if(actorCrossThreadProperties.ShareBuffersForVR)
	{
		if(View->StereoPass==eSSP_LEFT_EYE)
		{
			//whichever one comes first, use it to fill the buffers, then: 
		//	style=style|VR_STYLE;
			uid=29435;
		}
		else if(View->StereoPass==eSSP_RIGHT_EYE)
		{
			//share the generated buffers for the other one.
		//	style=style|VR_STYLE|VR_STYLE_RIGHT_EYE;
			uid=29435;
		}
	}
    int view_id = StaticGetOrAddView((void*)uid);		// RVK: really need a unique view ident to pass here..
//	if(RenderParameters.bIsCubemap)
//		style=(PluginStyle)(style|CUBEMAP_STYLE);
	int4 depthViewport;
	depthViewport.x=RenderParameters.ViewportRect.Min.X;
	depthViewport.y=RenderParameters.ViewportRect.Min.Y;
	depthViewport.z=RenderParameters.ViewportRect.Width();
	depthViewport.w=RenderParameters.ViewportRect.Height();
	 // NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
	TArray<FVector2D>  MultiResConstants;
#ifdef NV_MULTIRES
	bool bMultiResEnabled = View->bMultiResEnabled;
	if (bMultiResEnabled)
	{
		SetMultiResConstants(MultiResConstants,View);
	}
#else
	bool bMultiResEnabled = false;
#endif
       // NVCHANGE_END: TrueSky + VR MultiRes Support
	
	
	RenderCloudShadow();
	//SCOPED_DRAW_EVENT(*RenderParameters.RHICmdList, StaticRenderFrame);// breaks on Xbox One 

	StaticRenderFrame( device,pContext,view_id, &(viewMatrix.M[0][0]), &(projMatrix.M[0][0])
		,depthTexture,colourTarget
		,depthViewport
		,&v
		,style
		,exposure
		,gamma
		,GFrameNumber
       // NVCHANGE_BEGIN: TrueSky + VR MultiRes Support
       ,(bMultiResEnabled ? (const float*)( MultiResConstants.GetData()) : nullptr)
       // NVCHANGE_END: TrueSky + VR MultiRes Support
		);
	
	RenderCloudShadow();
	if(GFrameNumber!=LastFrameNumber)
	{
		// TODO: This should only be done once per frame.
		UpdateTrueSkyLights(RenderParameters);
	//	StaticSetRenderTexture("Cubemap", nullptr);
		LastFrameNumber=GFrameNumber;
	}
	
// Fill in colours requested by the editor plugin.
	if(colourTableRequests.size())
	{
		for(auto i:colourTableRequests)
		{
			unsigned uidc=i.first;
			ColourTableRequest *req=i.second;
			if(req&&req->valid)
				continue;
			delete [] req->data;
			req->data=new float[4*req->x*req->y*req->z];
			if(StaticFillColourTable(uidc,req->x,req->y,req->z,req->data))
			{
				UE_LOG(TrueSky,Display, TEXT("Colour table filled!"));
				req->valid=true;
				break;
			}
			else
			{
				UE_LOG(TrueSky,Display, TEXT("Colour table not filled!"));
				req->valid=false;
				continue;
			}
		}

		// tell the editor that the work is done.
		if(TrueSkyEditorCallback)
			TrueSkyEditorCallback();
	}
#if PLATFORM_PS4 || PLATFORM_XBOXONE
	if(GlobalOverlayFlag)
	{
//		StaticRenderOverlays(device,pContext,depthTexture,  &(viewMatrix.M[0][0]),&(projMatrix.M[0][0]),view_id);
	}
#endif
	if(exportNext)
	{
		StaticExportCloudLayerToGeometry(exportFilenameUtf8,0);
		exportNext=false;
	}
#endif
	// TODO: What we should really do now is to unset all resources on RenderParameters.RHICmdList, just in case
	// UE THINKS that it still has values set, that are now no longer really set in the (say) DX11 API.
//	RenderParameters.RHICmdList->FlushComputeShaderCache();
}

void FTrueSkyPlugin::OnDebugTrueSky(class UCanvas* Canvas, APlayerController*)
{
	const FColor OldDrawColor = Canvas->DrawColor;
	const FFontRenderInfo FontRenderInfo = Canvas->CreateFontRenderInfo(false, true);

	Canvas->SetDrawColor(FColor::White);

	UFont* RenderFont = GEngine->GetSmallFont();
	Canvas->DrawText(RenderFont, FString("trueSKY Debug Display"), 0.3f, 0.3f, 1.f, 1.f, FontRenderInfo);

	Canvas->SetDrawColor(OldDrawColor);
}

void FTrueSkyPlugin::ShutdownModule()
{
	if(StaticShutDownInterface)
		StaticShutDownInterface();
	if(StaticSetGraphicsDevice)
#if PLATFORM_PS4
		StaticSetGraphicsDevice(nullptr, GraphicsDevicePS4, GraphicsEventShutdown);
#else
		StaticSetGraphicsDevice(nullptr, GraphicsDeviceD3D11, GraphicsEventShutdown);
#endif
	delete PathEnv;
	PathEnv = nullptr;
	RendererInitialized=false;
	sequenceInUse=nullptr;
	if(StaticSetDebugOutputCallback)
		StaticSetDebugOutputCallback(nullptr);
}
#ifdef _MSC_VER
/** Returns environment variable value */
static wchar_t* GetEnvVariable( const wchar_t* const VariableName, int iEnvSize = 1024)
{
	wchar_t* Env = new wchar_t[iEnvSize];
	check( Env );
	memset(Env, 0, iEnvSize * sizeof(wchar_t));
	if ( (int)GetEnvironmentVariableW(VariableName, Env, iEnvSize) > iEnvSize )
	{
		delete [] Env;
		Env = nullptr;
	}
	else if ( wcslen(Env) == 0 )
	{
		return nullptr;
	}
	return Env;
}

#endif
/** Takes Base path, concatenates it with Relative path */
static const TCHAR* ConstructPath(const TCHAR* const BasePath, const TCHAR* const RelativePath)
{
	if ( BasePath )
	{
		const int iPathLen = 1024;
		TCHAR* const NewPath = new TCHAR[iPathLen];
		check( NewPath );
		wcscpy_s( NewPath, iPathLen, BasePath );
		if ( RelativePath )
		{
			wcscat_s( NewPath, iPathLen, RelativePath );
		}
		return NewPath;
	}
	return nullptr;
}
/** Takes Base path, concatenates it with Relative path and returns it as 8-bit char string */
static std::string ConstructPathUTF8(const TCHAR* const BasePath, const TCHAR* const RelativePath)
{
	if ( BasePath )
	{
		const int iPathLen = 1024;

		TCHAR* const NewPath = new TCHAR[iPathLen];
		check( NewPath );
		wcscpy_s( NewPath, iPathLen, BasePath );
		if ( RelativePath )
		{
			wcscat_s( NewPath, iPathLen, RelativePath );
		}

		char* const utf8NewPath = new char[iPathLen];
		check ( utf8NewPath );
		memset(utf8NewPath, 0, iPathLen);
#if PLATFORM_PS4
		size_t res=wcstombs(utf8NewPath, NewPath, iPathLen);
#else
		WideCharToMultiByte( CP_UTF8, 0, NewPath, iPathLen, utf8NewPath, iPathLen, nullptr, nullptr );
#endif

		delete [] NewPath;
		std::string ret=utf8NewPath;
		delete [] utf8NewPath;
		return ret;
	}
	return "";
}

bool CheckDllFunction(void *fn,FString &str,const char *fnName)
{
	bool res=(fn!=0);
	if(!res)
	{
		if(!str.IsEmpty())
			str+=", ";
		str+=fnName;
	}
	return res;
}
#if PLATFORM_PS4
#define MISSING_FUNCTION(fnName) (!CheckDllFunction((void*)fnName,failed_functions, #fnName))
#else
#define MISSING_FUNCTION(fnName) (!CheckDllFunction(fnName,failed_functions, #fnName))
#endif
// Reimplement because it's not there in the Xbox One version
//FWindowsPlatformProcess::GetDllHandle
moduleHandle GetDllHandle( const TCHAR* Filename )
{
	check(Filename);
#if PLATFORM_PS4
//moduleHandle ImageFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ImageFullPath);
	moduleHandle m= sceKernelLoadStartModule(TCHAR_TO_ANSI(Filename), 0, 0, 0, nullptr, nullptr);
	switch(m)
	{
		case SCE_KERNEL_ERROR_EINVAL:
			UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllHandle error: 0x80020016 flags or pOpt is invalid "));
			return 0;
		case SCE_KERNEL_ERROR_ENOENT:			 
			UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllHandle error: 0x80020002 File specified in moduleFileName does not exist "));
			return 0;
		case SCE_KERNEL_ERROR_ENOEXEC:			  
			UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllHandle error:  0x80020008 Cannot load because of abnormal file format "));
			return 0;
		case SCE_KERNEL_ERROR_ENOMEM:			  
			UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllHandle error: 0x8002000c Cannot load because it is not possible to allocate memory "));
			return 0;
		case SCE_KERNEL_ERROR_EFAULT:			  
			UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllHandle error: 0x8002000e moduleFileName points to invalid memory "));
			return 0;
		case SCE_KERNEL_ERROR_EAGAIN:			 
			UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllHandle error: 0x80020023 Cannot load because of insufficient resources "));
			return 0;
		default:
			break;
	};
	return m;
#else
	return ::LoadLibraryW(Filename);
#endif
}

// And likewise for GetDllExport:
void* GetDllExport( moduleHandle DllHandle, const TCHAR* ProcName )
{
	check(DllHandle);
	check(ProcName);
#if PLATFORM_PS4
	void *addr=nullptr;
	int result=sceKernelDlsym( DllHandle,
								TCHAR_TO_ANSI(ProcName),
								&addr);
	/*SCE_KERNEL_ERROR_ESRCH 0x80020003 handle is invalid, or symbol specified in symbol is not exported 
SCE_KERNEL_ERROR_EFAULT 0x8002000e symbol or addrp address is invalid 

	*/
	if(result==SCE_KERNEL_ERROR_ESRCH)
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllExport got error: SCE_KERNEL_ERROR_ESRCH, which means 'handle is invalid, or symbol specified in symbol is not exported'"));
	}
	else if(result==SCE_KERNEL_ERROR_EFAULT)
	{
		UE_LOG_ONCE(TrueSky, Warning, TEXT("GetDllExport got error: SCE_KERNEL_ERROR_EFAULT"));
	}
	return addr;
#elif PLATFORM_WINDOWS || PLATFORM_XBOXONE
	return (void*)::GetProcAddress( (HMODULE)DllHandle, TCHAR_TO_ANSI(ProcName) );
#else
	return nullptr;
#endif
}
#define GET_FUNCTION(fnName) {fnName= (F##fnName)GetDllExport(DllHandle, TEXT(#fnName) );if(fnName==nullptr){UE_LOG(TrueSky, Warning, TEXT("DLL Export function %s not found."),TEXT(#fnName));}}

bool FTrueSkyPlugin::InitRenderingInterface(  )
{
	InitPaths();
	static bool failed_once = false;
	FString EnginePath=FPaths::EngineDir();
#if PLATFORM_XBOXONE
	FString PlatformName=TEXT("XboxOne");
	FString pluginFilename=TEXT("TrueSkyPluginRender_MD");
	FString debugExtension=TEXT("d");
	FString dllExtension=TEXT(".dll");
#endif
#if PLATFORM_PS4
	FString PlatformName=TEXT("ps4");
	FString pluginFilename=TEXT("trueskypluginrender");
	FString debugExtension=TEXT("-debug");
	FString dllExtension=TEXT(".prx");
#endif
#if PLATFORM_WINDOWS
	FString PlatformName=TEXT("Win64");
	FString pluginFilename=TEXT("TrueSkyPluginRender_MT");
	FString debugExtension=TEXT("d");
	FString dllExtension=TEXT(".dll");
#endif
#ifndef NDEBUG // UE_BUILD_DEBUG //doesn't work... why?
	pluginFilename+=debugExtension;
#endif
	pluginFilename+=dllExtension;
	FString DllPath(FPaths::Combine(*EnginePath,*(TEXT("/binaries/thirdparty/simul/")+PlatformName)));
	FString DllFilename(FPaths::Combine(*DllPath,*pluginFilename));
#if PLATFORM_PS4
	if(!FPlatformFileManager::Get().GetPlatformFile().FileExists(*DllFilename))
	{
		//   ../../../engine/plugins/trueskyplugin/alternatebinariesdir/ps4/trueskypluginrender.prx
		// For SOME reason, UE moves all prx's to the root/prx folder...??
		DllPath=TEXT("prx/");
		DllFilename=FPaths::Combine(*DllPath,*pluginFilename);
		if(!FPlatformFileManager::Get().GetPlatformFile().FileExists(*DllFilename))
		{
			DllPath=FPaths::Combine(*EnginePath,TEXT("../prx/"));
			DllFilename=FPaths::Combine(*DllPath,*pluginFilename);
		}
	}
#endif
	if(!FPlatformFileManager::Get().GetPlatformFile().FileExists(*DllFilename))
	{
		if (!failed_once)
			UE_LOG(TrueSky, Warning, TEXT("%s not found."), *DllFilename);
		failed_once = true;
		return false;
	}
#if PLATFORM_PS4
	IFileHandle *fh=FPlatformFileManager::Get().GetPlatformFile().OpenRead(*DllFilename);
	delete fh;
	DllFilename	=FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForRead(*DllFilename);
#endif
	moduleHandle const DllHandle = GetDllHandle((const TCHAR*)DllFilename.GetCharArray().GetData() );
	if(DllHandle==0)
	{
		if(!failed_once)
			UE_LOG(TrueSky, Warning, TEXT("Failed to load %s"), (const TCHAR*)DllFilename.GetCharArray().GetData());
		failed_once = true;
		return false;
	}
	if ( DllHandle != 0 )
	{
		StaticInitInterface				=(FStaticInitInterface)GetDllExport(DllHandle, TEXT("StaticInitInterface") );
		StaticSetMemoryInterface		=(FStaticSetMemoryInterface)GetDllExport(DllHandle, TEXT("StaticSetMemoryInterface") );
		StaticShutDownInterface			=(FStaticShutDownInterface)GetDllExport(DllHandle, TEXT("StaticShutDownInterface") );
		StaticSetDebugOutputCallback	=(FStaticSetDebugOutputCallback)GetDllExport(DllHandle,TEXT("StaticSetDebugOutputCallback"));
		StaticSetGraphicsDevice			=(FStaticSetGraphicsDevice)GetDllExport(DllHandle, TEXT("StaticSetGraphicsDevice") );
		StaticPushPath					=(FStaticPushPath)GetDllExport(DllHandle, TEXT("StaticPushPath") );
		GET_FUNCTION(StaticSetFileLoader);
		GET_FUNCTION(StaticGetLightningBolts);
		GET_FUNCTION(StaticSpawnLightning);
		GET_FUNCTION(StaticFillColourTable);
		GET_FUNCTION(GetSimulVersion);
		
		int major=4,minor=0,build=0;
		if(GetSimulVersion)
		{
			GetSimulVersion(&major,&minor,&build);
			simulVersion=ToSimulVersion(major,minor,build);
			UE_LOG(TrueSky, Display, TEXT("Simul version %d.%d build %d"), major,minor,build);
		}
		GET_FUNCTION(StaticGetOrAddView);
		GET_FUNCTION(StaticRenderFrame);
		GET_FUNCTION(StaticCopySkylight);
		GET_FUNCTION(StaticRenderOverlays);
			
		GET_FUNCTION(StaticOnDeviceChanged);
		GET_FUNCTION(StaticTick);
		GET_FUNCTION(StaticGetEnvironment);
		GET_FUNCTION(StaticSetSequence);
		GET_FUNCTION(StaticGetRenderInterfaceInstance);

		GET_FUNCTION(StaticSetPointLight);
		GET_FUNCTION(StaticCloudPointQuery);
		GET_FUNCTION(StaticCloudLineQuery);
		GET_FUNCTION(StaticSetRenderTexture);
		GET_FUNCTION(StaticSetMatrix4x4);
		GET_FUNCTION(StaticSetRender);
		GET_FUNCTION(StaticSetRenderBool);
		GET_FUNCTION(StaticGetRenderBool);
		GET_FUNCTION(StaticSetRenderFloat);
		GET_FUNCTION(StaticGetRenderFloat);
		GET_FUNCTION(StaticSetRenderInt);
		GET_FUNCTION(StaticGetRenderInt);
					
		GET_FUNCTION(StaticGetRenderString);		
		GET_FUNCTION(StaticSetRenderString);		

		GET_FUNCTION(StaticExportCloudLayerToGeometry);

		GET_FUNCTION(StaticTriggerAction);

		StaticSetKeyframeFloat			=(FStaticSetKeyframeFloat)GetDllExport(DllHandle, TEXT("StaticRenderKeyframeSetFloat"));
		StaticGetKeyframeFloat			=(FStaticGetKeyframeFloat)GetDllExport(DllHandle, TEXT("StaticRenderKeyframeGetFloat"));
		StaticSetKeyframeInt			=(FStaticSetKeyframeInt)GetDllExport(DllHandle,	TEXT("StaticRenderKeyframeSetInt"));
		StaticGetKeyframeInt			=(FStaticGetKeyframeInt)GetDllExport(DllHandle,	TEXT("StaticRenderKeyframeGetInt"));

		GET_FUNCTION(StaticGetRenderFloatAtPosition);
		
		FString failed_functions;
		int num_fails=MISSING_FUNCTION(StaticInitInterface)
			+MISSING_FUNCTION(StaticSetMemoryInterface)
			+MISSING_FUNCTION(StaticShutDownInterface)
			+MISSING_FUNCTION(StaticSetDebugOutputCallback)
			+MISSING_FUNCTION(StaticSetGraphicsDevice)
			+MISSING_FUNCTION(StaticPushPath)
			+MISSING_FUNCTION(StaticSetFileLoader)
			+MISSING_FUNCTION(StaticRenderFrame)
			+MISSING_FUNCTION(StaticGetLightningBolts)
			+MISSING_FUNCTION(StaticSpawnLightning)
			+MISSING_FUNCTION(StaticFillColourTable)
			+MISSING_FUNCTION(StaticRenderOverlays)
			+MISSING_FUNCTION(StaticGetOrAddView)
			+MISSING_FUNCTION(StaticGetEnvironment)
			+MISSING_FUNCTION(StaticSetSequence)
			+MISSING_FUNCTION(StaticGetRenderInterfaceInstance)
			+MISSING_FUNCTION(StaticSetPointLight)
			+MISSING_FUNCTION(StaticCloudPointQuery)
			+MISSING_FUNCTION(StaticCloudLineQuery)
			+MISSING_FUNCTION(StaticSetRenderTexture)
			+MISSING_FUNCTION(StaticSetRenderBool)
			+MISSING_FUNCTION(StaticGetRenderBool)
			+MISSING_FUNCTION(StaticTriggerAction)
			+MISSING_FUNCTION(StaticSetRenderFloat)
			+MISSING_FUNCTION(StaticGetRenderFloat)
			+MISSING_FUNCTION(StaticSetRenderString)
			+MISSING_FUNCTION(StaticGetRenderString)
			+MISSING_FUNCTION(StaticExportCloudLayerToGeometry)
			+MISSING_FUNCTION(StaticSetRenderInt)
			+MISSING_FUNCTION(StaticGetRenderInt)
			+MISSING_FUNCTION(StaticSetKeyframeFloat)
			+MISSING_FUNCTION(StaticGetKeyframeFloat)
			+MISSING_FUNCTION(StaticSetKeyframeInt)
			+MISSING_FUNCTION(StaticGetKeyframeInt)
			+MISSING_FUNCTION(StaticSetMatrix4x4)
			+MISSING_FUNCTION(StaticGetRenderFloatAtPosition);
		if(num_fails>0)
		{
			static bool reported=false;
			if(!reported)
			{
				UE_LOG(TrueSky, Error
					,TEXT("Can't initialize the trueSKY rendering plugin dll because %d functions were not found - please update TrueSkyPluginRender_MT.dll.\nThe missing functions are %s.")
					,num_fails
					,*failed_functions
					);
				reported=true;
			}
			//missing dll functions... cancel initialization
			SetRenderingEnabled(false);
			return false;
		}
		UE_LOG(TrueSky, Display, TEXT("Loaded trueSKY dynamic library %s."), *DllFilename);
#if PLATFORM_PS4
		StaticSetMemoryInterface(&ps4MemoryAllocator);
#endif
		StaticSetDebugOutputCallback(LogCallback);
		return true;
	}
	return false;
}

bool FTrueSkyPlugin::EnsureRendererIsInitialized()
{
	ERRNO_CHECK
	if(!RendererInitialized)
	{
		if(InitRenderingInterface())
			RendererInitialized=true;
		if(!RendererInitialized)
			return false;
		void *device=GetPlatformDevice();
		
		if( device != nullptr )
		{
			FString EnginePath=FPaths::EngineDir();
			StaticSetFileLoader(&ue4SimulFileLoader);
#if PLATFORM_PS4
			FString shaderbin(FPaths::Combine(*EnginePath, TEXT("plugins/trueskyplugin/shaderbin/ps4")));
#elif PLATFORM_XBOXONE
			FString shaderbin(FPaths::Combine(*EnginePath, TEXT("plugins/trueskyplugin/shaderbin/XboxOne")));
#else
			FString shaderbin=FPaths::EngineDir()+L"/plugins/trueskyplugin/shaderbin/Win64";
#endif
			FString texpath(FPaths::Combine(*EnginePath, TEXT("../../../engine/plugins/trueskyplugin/resources/media/textures")));
			StaticPushPath("TexturePath", FStringToUtf8(texpath).c_str());
			FString texpath2(TEXT("../../../engine/plugins/trueskyplugin/textures"));
			StaticPushPath("TexturePath", FStringToUtf8(texpath2).c_str());
#if PLATFORM_WINDOWS
			if(haveEditor)
			{
				StaticPushPath("ShaderPath",FStringToUtf8(trueSkyPluginPath+"\\Resources\\Platform\\DirectX11\\HLSL").c_str());
				StaticPushPath("TexturePath",FStringToUtf8(trueSkyPluginPath+"\\Resources\\Media\\Textures").c_str());
			}
			else
			{
				static FString gamePath="../../";
				StaticPushPath("ShaderPath",FStringToUtf8(gamePath+L"\\Content\\TrueSkyPlugin\\Platform\\DirectX11\\HLSL").c_str());
				StaticPushPath("TexturePath",FStringToUtf8(gamePath+L"\\Content\\TrueSkyPlugin\\Media\\Textures").c_str());
			}
#endif
			StaticPushPath("ShaderBinaryPath",FStringToUtf8(shaderbin).c_str());
#ifdef _MSC_VER
			// IF there's a "SIMUL" env variable, we can build shaders direct from there:
			wchar_t *SimulPath = GetEnvVariable(L"SIMUL");
			if(SimulPath)
				StaticPushPath("ShaderPath", ConstructPathUTF8( SimulPath, L"\\Platform\\DirectX11\\HLSL" ).c_str());
			delete [] SimulPath;
#endif
		}
#if PLATFORM_PS4
		StaticSetGraphicsDevice(device, GraphicsDeviceType::GraphicsDevicePS4, GraphicsEventInitialize);
#else
		StaticSetGraphicsDevice(device, GraphicsDeviceType::GraphicsDeviceD3D11, GraphicsEventInitialize);
#endif
		if( device != nullptr )
		{
			RendererInitialized = true;
		}
		else
			RendererInitialized=false;
	}
	if(RendererInitialized)
	{
		StaticInitInterface();
		RenderingEnabled=actorCrossThreadProperties.Visible;
	}
	return RendererInitialized;
}

void FTrueSkyPlugin::SetPointLight(int id,FLinearColor c,FVector pos,float min_radius,float max_radius)
{
	if(!StaticSetPointLight)
		return;
	criticalSection.Lock();
	ActorCrossThreadProperties *A=GetActorCrossThreadProperties();
	FVector ts_pos=UEToTrueSkyPosition(A->Transform,A->MetresPerUnit,pos);
	min_radius	*=actorCrossThreadProperties.MetresPerUnit;
	max_radius	*=actorCrossThreadProperties.MetresPerUnit;
	StaticSetPointLight(id,(const float*)&pos,min_radius,max_radius,(const float*)&c);
	criticalSection.Unlock();
}

ITrueSkyPlugin::VolumeQueryResult FTrueSkyPlugin::GetStateAtPosition(int32 queryId,FVector pos)
{
	VolumeQueryResult res;

	if(!StaticCloudPointQuery)
	{
		memset(&res,0,sizeof(res));
		return res;
	}
	criticalSection.Lock();
	pos*=actorCrossThreadProperties.MetresPerUnit;
	std::swap(pos.Y,pos.X);
	StaticCloudPointQuery(queryId,(const float*)&pos,&res);
	criticalSection.Unlock();
	return res;
}

float FTrueSkyPlugin::GetCloudinessAtPosition(int32 queryId,FVector pos)
{
	if(!StaticCloudPointQuery)
		return 0.f;
	criticalSection.Lock();
	VolumeQueryResult res;
	pos*=actorCrossThreadProperties.MetresPerUnit;
	std::swap(pos.Y,pos.X);
	StaticCloudPointQuery(queryId,(const float*)&pos,&res);
	criticalSection.Unlock();
	return FMath::Clamp(res.density, 0.0f, 1.0f);
}

float FTrueSkyPlugin::CloudLineTest(int32 queryId,FVector StartPos,FVector EndPos)
{
	if(!StaticCloudLineQuery)
		return 0.f;
	criticalSection.Lock();
	LineQueryResult res;
	StartPos*=actorCrossThreadProperties.MetresPerUnit;
	EndPos*=actorCrossThreadProperties.MetresPerUnit;
	std::swap(StartPos.Y,StartPos.X);
	std::swap(EndPos.Y,EndPos.X);
	StaticCloudLineQuery(queryId,(const float*)&StartPos,(const float*)&EndPos,&res);
	criticalSection.Unlock();
	return FMath::Clamp(res.density, 0.0f, 1.0f);
}

void FTrueSkyPlugin::InitPaths()
{
#ifdef _MSC_VER
	if ( PathEnv == nullptr )
	{
		static FString path;
		path= GetEnvVariable(L"PATH");
		FString DllPath(FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Simul/Win64")));
			
		path=(DllPath+L";")+path;
		PathEnv=(const TCHAR*)path.GetCharArray().GetData() ;
		SetEnvironmentVariable( L"PATH", PathEnv);
	}
#endif
}

void FTrueSkyPlugin::OnToggleRendering()
{
	if ( UTrueSkySequenceAsset* const ActiveSequence = GetActiveSequence() )
	{
		SetRenderingEnabled(!RenderingEnabled);
		if(RenderingEnabled)
		{
			SequenceChanged();
		}
	}
	else if(RenderingEnabled)
	{
		// no active sequence, so disable rendering
		SetRenderingEnabled(false);
	}
}

void FTrueSkyPlugin::OnUIChangedSequence()
{
	SequenceChanged();
	// Make update instant, instead of gradual, if it's a change the user made.
	TriggerAction("Reset");
}

void FTrueSkyPlugin::OnUIChangedTime(float t)
{
	UTrueSkySequenceAsset* const ActiveSequence = GetActiveSequence();
	if(ActiveSequence)
	{
		SetRenderFloat("time",t);
	}
}
	
void FTrueSkyPlugin::ExportCloudLayer(const FString& fname,int index)
{
	exportNext=true;
	std::string name=FStringToUtf8(fname);
	strcpy_s(exportFilenameUtf8,100,name.c_str());
}

void FTrueSkyPlugin::SequenceChanged()
{
	if(!RenderingEnabled)
		return;
	UTrueSkySequenceAsset* const ActiveSequence = GetActiveSequence();
	if(ActiveSequence&&StaticSetSequence)
	{
		if(ActiveSequence->SequenceText.Num()>0)
		{
			std::string SequenceInputText;
			SequenceInputText = std::string((const char*)ActiveSequence->SequenceText.GetData());
			if(StaticSetSequence(SequenceInputText)==0)
				sequenceInUse=ActiveSequence;
		}
	}
}

IMPLEMENT_TOGGLE(ShowFades)
IMPLEMENT_TOGGLE(ShowCompositing)
IMPLEMENT_TOGGLE(Show3DCloudTextures)
IMPLEMENT_TOGGLE(Show2DCloudTextures)

bool FTrueSkyPlugin::IsToggleRenderingEnabled()
{
	if(GetActiveSequence())
	{
		return true;
	}
	// No active sequence found!
	SetRenderingEnabled(false);
	return false;
}

bool FTrueSkyPlugin::IsToggleRenderingChecked()
{
	return RenderingEnabled;
}

void FTrueSkyPlugin::OnAddSequence()
{
	ULevel* const Level = GWorld->PersistentLevel;
	ATrueSkySequenceActor* SequenceActor = nullptr;
	// Check for existing sequence actor
	for(int i = 0; i < Level->Actors.Num() && SequenceActor == nullptr; i++)
	{
		SequenceActor = Cast<ATrueSkySequenceActor>( Level->Actors[i] );
	}
	//if ( SequenceActor == nullptr )
	{
		// Add sequence actor
		SequenceActor=GWorld->SpawnActor<ATrueSkySequenceActor>(ATrueSkySequenceActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	}
//	else
	{
		// Sequence actor already exists -- error message?
	}
}

void FTrueSkyPlugin::OnSequenceDestroyed()
{
}

bool FTrueSkyPlugin::IsAddSequenceEnabled()
{
	// Returns false if TrueSkySequenceActor already exists!
	ULevel* const Level = GWorld->PersistentLevel;
	for(int i=0;i<Level->Actors.Num();i++)
	{
		if ( Cast<ATrueSkySequenceActor>(Level->Actors[i]) )
			return false;
	}
	return true;
}

void FTrueSkyPlugin::UpdateFromActor()
{
	if(sequenceInUse!=GetActiveSequence())
		SequenceChanged();
	if(actorCrossThreadProperties.Destroyed)
		actorCrossThreadProperties.Visible=false;
	if(actorCrossThreadProperties.Visible!=RenderingEnabled)
	{
		OnToggleRendering();
	}
	if(RenderingEnabled&&RendererInitialized)
	{
		if(simulVersion>=SIMUL_4_1)
		{
			SetRenderInt("InterpolationMode",(int)actorCrossThreadProperties.InterpolationMode);
			SetRenderFloat("MinimumStarPixelSize",(int)actorCrossThreadProperties.MinimumStarPixelSize);
			SetRenderFloat("InterpolationTimeSeconds",actorCrossThreadProperties.InterpolationTimeSeconds);
			//SetRenderInt("InterpolationSubdivisions",actorCrossThreadProperties.InterpolationSubdivisions);
			SetRenderInt("PrecipitationOptions",actorCrossThreadProperties.PrecipitationOptions);
		}

		SetRenderFloat("SimpleCloudShadowing",actorCrossThreadProperties.SimpleCloudShadowing);
		SetRenderFloat("SimpleCloudShadowSharpness",actorCrossThreadProperties.SimpleCloudShadowSharpness);
		SetRenderFloat("CloudThresholdDistanceKm", actorCrossThreadProperties.CloudThresholdDistanceKm);
		if(actorCrossThreadProperties.Playing)
		{
			SetRenderFloat("RealTime", actorCrossThreadProperties.Time); 
		}
		else
		{
			TriggerAction("CalcRealTime"); 
		}
		if(simulVersion>=SIMUL_4_1)
		{
			SetRenderInt("MaximumCubemapResolution",actorCrossThreadProperties.MaximumResolution);
			SetRenderFloat("DepthSamplingPixelRange",actorCrossThreadProperties.DepthSamplingPixelRange);
		}
		else
		{
			SetRenderInt("Downscale",std::min(8,std::max(1,1920/actorCrossThreadProperties.MaximumResolution)));
		}
		SetRenderInt("Amortization",actorCrossThreadProperties.Amortization);
		SetRenderInt("AtmosphericsAmortization", actorCrossThreadProperties.AtmosphericsAmortization);
		SetRenderBool("DepthBlending", actorCrossThreadProperties.DepthBlending);

		int num_l=StaticGetLightningBolts(actorCrossThreadProperties.lightningBolts,4);
		for(int i=0;i<num_l;i++)
		{
			LightningBolt *l=&actorCrossThreadProperties.lightningBolts[i];
			FVector u=TrueSkyToUEPosition(actorCrossThreadProperties.Transform,actorCrossThreadProperties.MetresPerUnit,FVector(l->pos[0],l->pos[1],l->pos[2]));
			FVector v=TrueSkyToUEPosition(actorCrossThreadProperties.Transform,actorCrossThreadProperties.MetresPerUnit,FVector(l->endpos[0],l->endpos[1],l->endpos[2]));
			l->pos[0]=u.X;
			l->pos[1]=u.Y;
			l->pos[2]=u.Z;
			l->endpos[0]=v.X;
			l->endpos[1]=v.Y;
			l->endpos[2]=v.Z;
		}
		if(actorCrossThreadProperties.Reset)
		{
			TriggerAction("Reset");
			actorCrossThreadProperties.Reset=false;
		}
	}
}

UTrueSkySequenceAsset* FTrueSkyPlugin::GetActiveSequence()
{
	return actorCrossThreadProperties.activeSequence;
}

FMatrix FTrueSkyPlugin::UEToTrueSkyMatrix(bool apply_scale) const
{
	FMatrix TsToUe	=actorCrossThreadProperties.Transform.ToMatrixWithScale();
	FMatrix UeToTs	=TsToUe.InverseFast();
	for(int i=apply_scale?0:3;i<4;i++)
	{
		for(int j=0;j<3;j++)
		{
			UeToTs.M[i][j]			*=actorCrossThreadProperties.MetresPerUnit;
		}
	}

	for(int i=0;i<4;i++)
		std::swap(UeToTs.M[i][0],UeToTs.M[i][1]);
	return UeToTs;
}

FMatrix FTrueSkyPlugin::TrueSkyToUEMatrix(bool apply_scale) const
{
	FMatrix TsToUe	=actorCrossThreadProperties.Transform.ToMatrixWithScale();
	for(int i=0;i<4;i++)
		std::swap(TsToUe.M[i][0],TsToUe.M[i][1]);
	for(int i=apply_scale?0:3;i<4;i++)
	{
		for(int j=0;j<3;j++)
		{
			TsToUe.M[i][j]			/=actorCrossThreadProperties.MetresPerUnit;
		}
	}
	return TsToUe;
}
namespace simul
{
	namespace unreal
	{
		FVector UEToTrueSkyPosition(const FTransform &tr,float MetresPerUnit,FVector ue_pos) 
		{
			FMatrix TsToUe	=tr.ToMatrixWithScale();
			FVector ts_pos	=tr.InverseTransformPosition(ue_pos);
			ts_pos			*=MetresPerUnit;
			std::swap(ts_pos.Y,ts_pos.X);
			return ts_pos;
		}

		FVector TrueSkyToUEPosition(const FTransform &tr,float MetresPerUnit,FVector ts_pos) 
		{
			ts_pos			/=actorCrossThreadProperties.MetresPerUnit;
			std::swap(ts_pos.Y,ts_pos.X);
			FVector ue_pos	=tr.TransformPosition(ts_pos);
			return ue_pos;
		}

		FVector UEToTrueSkyDirection(const FTransform &tr,FVector ue_dir) 
		{
			FMatrix TsToUe	=tr.ToMatrixNoScale();
			FVector ts_dir	=tr.InverseTransformVectorNoScale(ue_dir);
			std::swap(ts_dir.Y,ts_dir.X);
			return ts_dir;
		}

		FVector TrueSkyToUEDirection(const FTransform &tr,FVector ts_dir) 
		{
			std::swap(ts_dir.Y,ts_dir.X);
			FVector ue_dir	=tr.TransformVectorNoScale(ts_dir);
			return ue_dir;
		}
	}
}