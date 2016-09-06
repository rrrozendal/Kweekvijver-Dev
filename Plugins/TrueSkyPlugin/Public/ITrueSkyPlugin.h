// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModuleManager.h"
#include "Engine.h"
namespace simul
{
	namespace unreal
	{
		extern FVector UEToTrueSkyPosition(const FTransform &tr,float MetresPerUnit,FVector ue_pos) ;
		extern FVector TrueSkyToUEPosition(const FTransform &tr,float MetresPerUnit,FVector ts_pos) ;
		extern FVector UEToTrueSkyDirection(const FTransform &tr,FVector ue_dir) ;
		extern FVector TrueSkyToUEDirection(const FTransform &tr,FVector ts_dir) ;
	}
}


// Definitions for Simul version number:
#define MAKE_SIMUL_VERSION(major,minor,build) ((major<<24)+(minor<<16)+build)

typedef unsigned SimulVersion;

static const SimulVersion SIMUL_4_0=MAKE_SIMUL_VERSION(4,0,0);
static const SimulVersion SIMUL_4_1=MAKE_SIMUL_VERSION(4,1,0);

inline SimulVersion ToSimulVersion(int major,int minor,int build=0)
{
	return MAKE_SIMUL_VERSION(major,minor,build);
}

class ATrueSkySequenceActor;
/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules 
 * within this plugin.
 */
class ITrueSkyPlugin : public IModuleInterface
{
public:
	struct ColourTableRequest
	{
		ColourTableRequest():uid(0),x(0),y(0),z(0),valid(false),data(NULL)
		{
		}
		unsigned uid;
		int x,y,z;	// sizes of the table
		bool valid;
		float *data;
	};
	//! The result struct for a point or volume query.
	struct VolumeQueryResult
	{
		float pos_m[3];
		int valid;
		float density;
		float direct_light;
		float indirect_light;
		float ambient_light;
		float precipitation;
	};
	//! The result struct for a line query.
	struct LineQueryResult
	{
		float pos1[3];
		int valid;
		float pos2[3];
		float density;
		float visibility;
		float optical_thickness_km;
		float first_contact_km;
	};
	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline ITrueSkyPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked< ITrueSkyPlugin >( "TrueSkyPlugin" );
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded( "TrueSkyPlugin" );
	}
	virtual void	SetPointLight(int id,FLinearColor c,FVector pos,float min_radius,float max_radius)=0;
	virtual float	CloudLineTest(int32 queryId,FVector StartPos,FVector EndPos) =0;
	virtual float	GetCloudinessAtPosition(int32 queryId,FVector pos)=0;
	virtual VolumeQueryResult		GetStateAtPosition(int32 queryId,FVector pos) =0;
	virtual void	SetRenderFloat(const FString& name, float value) = 0;
	virtual float	GetRenderFloat(const FString& name) const = 0;
	virtual float	GetRenderFloatAtPosition(const FString& name,FVector Pos) const = 0;
	virtual void	SetRender(const FString &fname,const TArray<FVariant> &params) = 0;
	virtual void	SetRenderInt(const FString& name, int value) = 0;
	virtual int		GetRenderInt(const FString& name) const = 0;
	virtual int		GetRenderInt(const FString& name,const TArray<FVariant> &params) const = 0;
	
	virtual void	SetKeyframeFloat(unsigned,const FString& name, float value) = 0;
	virtual float	GetKeyframeFloat(unsigned,const FString& name) const = 0;
	virtual void	SetKeyframeInt(unsigned,const FString& name, int value) = 0;
	virtual int		GetKeyframeInt(unsigned,const FString& name) const = 0;

	virtual void	SetRenderBool(const FString& name, bool value) = 0;
	virtual bool	GetRenderBool(const FString& name) const = 0;
	virtual void	SetRenderString(const FString& name, const FString&  value)=0;
	virtual FString	GetRenderString(const FString& name) const =0;
	virtual bool	TriggerAction(const FString& name) = 0;
	virtual void	SetRenderingEnabled(bool) = 0;
	
	virtual class	UTrueSkySequenceAsset* GetActiveSequence()=0;
	virtual void*	GetRenderEnvironment()=0;
	virtual void	OnToggleRendering() = 0;

	virtual void	OnUIChangedSequence()=0;
	virtual void	OnUIChangedTime(float)=0;
	
	virtual void	ExportCloudLayer(const FString& filenameUtf8,int index)=0;
	virtual UTexture *GetAtmosphericsTexture()=0;

	
	virtual void ClearCloudVolumes()=0;
	virtual void SetCloudVolume(int,FTransform tr,FVector ext)=0;
	
	virtual int32 SpawnLightning(FVector startpos,FVector endpos,float magnitude,FVector colour)=0;

	virtual void RequestColourTable(unsigned uid,int x,int y,int z)=0;
	virtual void ClearColourTableRequests()=0;
	virtual const ColourTableRequest *GetColourTable(unsigned uid) =0;

	typedef void (*FTrueSkyEditorCallback)();
	virtual void SetEditorCallback(FTrueSkyEditorCallback c) =0;

	virtual void InitializeDefaults(ATrueSkySequenceActor *)=0;

	virtual void AdaptViewMatrix(FMatrix &viewMatrix,bool editor_version=false)=0;

	virtual SimulVersion GetVersion() const=0;
};

