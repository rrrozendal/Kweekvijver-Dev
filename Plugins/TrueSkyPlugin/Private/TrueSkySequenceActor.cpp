#include "TrueSkyPluginPrivatePCH.h"
#include "ActorCrossThreadProperties.h"
#include "TrueSkySequenceActor.h"
#include "TrueSkyComponent.h"
#include "Kismet/KismetMaterialLibrary.h"
#include <atomic>
#include <map>
#include <algorithm>

static std::atomic<int> actors(0);

static const float PI_F=3.1415926536f;

std::map<UWorld*,ATrueSkySequenceActor *> worldCurrentActiveActors;
using namespace simul::unreal;

ATrueSkySequenceActor::ATrueSkySequenceActor(const class FObjectInitializer& PCIP)
	: Super(PCIP)
	,InterpolationMode(EInterpolationMode::FixedNumber)
	,InterpolationTimeSeconds(10.0f)
	,VelocityStreaks(true)
	,SimulationTime(false)
	,ActiveInEditor(true)
	,ActiveSequence(nullptr)
	,MoonTexture(nullptr)
	,CosmicBackgroundTexture(nullptr)
	,RainCubemap(nullptr)
	,InscatterRT(nullptr)
	,LossRT(nullptr)
	,CloudVisibilityRT(nullptr)
	,CloudShadowRT(nullptr)
	,Brightness(1.0f)
	,MetresPerUnit(0.01f)
	,SimpleCloudShadowing(0.5f)
	,SimpleCloudShadowSharpness(0.01f)
	,CloudThresholdDistanceKm(1.0f)
	,DownscaleFactor(2)
	,MaximumResolution(512)
	,DepthSamplingPixelRange(1.0f)
	,Amortization(1)
	,AtmosphericsAmortization(4)
	,DepthBlending(true)
	,MinimumStarPixelSize(2.0f)
	,Time(0.0f)
	,Visible(true)
	,MaxSunRadiance(150000.0f)
	,initializeDefaults(false)
	,wetness(0.0f)
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	PrimaryActorTick.bTickEvenWhenPaused	=true;
	PrimaryActorTick.bCanEverTick			=true;
	PrimaryActorTick.bStartWithTickEnabled	=true;
	SetTickGroup( TG_PrePhysics);
	SetActorTickEnabled(true);
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	actors++;
	// we must have TWO actors in order for one to be in a scene, because there's also a DEFAULT object!
	if(A&&actors>=2)
		A->Destroyed=false;
	latest_thunderbolt_id=0;
}

ATrueSkySequenceActor::~ATrueSkySequenceActor()
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	actors--;
	if(A&&actors<=1)
		A->Destroyed=true;
}

float ATrueSkySequenceActor::GetReferenceSpectralRadiance()
{
	float r	=ITrueSkyPlugin::Get().GetRenderFloat("ReferenceSpectralRadiance");
	return r;
}

UFUNCTION( BlueprintCallable,Category = TrueSky)
int32 ATrueSkySequenceActor::SpawnLightning(FVector pos1,FVector pos2,float magnitude,FVector colour)
{
	return ITrueSkyPlugin::Get().SpawnLightning(pos1,pos2,magnitude,colour);
}

int32 ATrueSkySequenceActor::GetLightning(FVector &start,FVector &end,float &magnitude,FVector &colour)
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	int i=0;
	{
		LightningBolt *L=&(A->lightningBolts[i]);
		if(L&&L->id!=0)
		{
			start=FVector(L->pos[0],L->pos[1],L->pos[2]);
			end=FVector(L->endpos[0],L->endpos[1],L->endpos[2]);
			magnitude=L->brightness;
			colour=FVector(L->colour[0],L->colour[1],L->colour[2]);
			return L->id;
		}
		else
		{
			magnitude=0.0f;
	}
	}
	return 0;
}

void ATrueSkySequenceActor::PostRegisterAllComponents()
{
	UWorld *world = GetWorld();
	// If any other Sequence Actor is already ActiveInEditor, make this new one Inactive.
	if (ActiveInEditor&&world!=nullptr&&world->WorldType == EWorldType::Editor)
	{
		bool in_list = false;
		for (TActorIterator<ATrueSkySequenceActor> Itr(world); Itr; ++Itr)
		{
			ATrueSkySequenceActor* SequenceActor = *Itr;
			if (SequenceActor == this)
			{
				in_list = true;
				continue;
			}
			if (SequenceActor->ActiveInEditor)
			{
				ActiveInEditor = false;
				break;
			}
		}
		if(in_list)
			worldCurrentActiveActors[world] = this;
	}
}
#define DEFAULT_PROPERTY(type,propname,filename) \
	if(propname==nullptr)\
	{\
		auto cls = StaticLoadObject(type::StaticClass(), nullptr, TEXT("/Game/TrueSky/"#filename));\
		propname=Cast<type>(cls);\
	}

void ATrueSkySequenceActor::PostInitProperties()
{
    Super::PostInitProperties();
	// We need to ensure this first, or we may get bad data in the transform:
	if(RootComponent)
		RootComponent->UpdateComponentToWorld();
	TransferProperties();
	UpdateVolumes();// no use
}

void ATrueSkySequenceActor::UpdateVolumes()
{
	//TArray< USceneComponent * > comps;
	TArray<UActorComponent *> comps;
	this->GetComponents(comps);
	//RootComponent->GetChildrenComponents(true,comps);
	ITrueSkyPlugin::Get().ClearCloudVolumes();
	for(int i=0;i<comps.Num();i++)
	{
		UActorComponent *a=comps[i];
		if(!a)
			continue;
	}
}

void ATrueSkySequenceActor::SetDefaultTextures()
{
	if(CloudShadowRT==nullptr)
	{
		auto cls = StaticLoadObject(UTextureRenderTarget2D::StaticClass(), nullptr, TEXT("/Game/TrueSky/CloudShadowRT"));
		if(cls==nullptr)
			cls = StaticLoadObject(UTextureRenderTarget2D::StaticClass(), nullptr, TEXT("/Game/TrueSky/CloudShadow"));
		UTextureRenderTarget2D * cloudShadow = Cast<UTextureRenderTarget2D>(cls);
		CloudShadowRT=cloudShadow;
	}
	DEFAULT_PROPERTY(UTexture2D,MoonTexture,Moon)
	DEFAULT_PROPERTY(UTexture2D,CosmicBackgroundTexture,MilkyWay)
	DEFAULT_PROPERTY(UTextureRenderTarget2D,InscatterRT,trueSKYInscatter)
	DEFAULT_PROPERTY(UTextureRenderTarget2D,LossRT,trueSKYLoss)
	DEFAULT_PROPERTY(UTextureRenderTarget2D,CloudVisibilityRT,cloudVisibilityRT)
}

void ATrueSkySequenceActor::PostLoad()
{
    Super::PostLoad();
	// We need to ensure this first, or we may get bad data in the transform:
	if(RootComponent)
		RootComponent->UpdateComponentToWorld();
	if(initializeDefaults)
	{
		SetDefaultTextures();
		initializeDefaults=false;
	}
	latest_thunderbolt_id=0;
	TransferProperties();
}

void ATrueSkySequenceActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();
	// We need to ensure this first, or we may get bad data in the transform:
	if(RootComponent)
		RootComponent->UpdateComponentToWorld();
	latest_thunderbolt_id=0;
	TransferProperties();
}

void ATrueSkySequenceActor::Destroyed()
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	if(!A)
		return;
	A->Destroyed=true;
	AActor::Destroyed();
}

void ATrueSkySequenceActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	latest_thunderbolt_id=0;
	ITrueSkyPlugin::Get().TriggerAction("Reset");
}

FString ATrueSkySequenceActor::GetProfilingText(int32 c,int32 g)
{
	ITrueSkyPlugin::Get().SetRenderInt("maxCpuProfileLevel",c);
	ITrueSkyPlugin::Get().SetRenderInt("maxGpuProfileLevel",g);
	return ITrueSkyPlugin::Get().GetRenderString("profiling");
}

void ATrueSkySequenceActor::SetTime( float value )
{
	ITrueSkyPlugin::Get().SetRenderFloat("time",value);
}

float ATrueSkySequenceActor::GetTime(  )
{
	return ITrueSkyPlugin::Get().GetRenderFloat("time");
}

float ATrueSkySequenceActor::GetFloat(FString name )
{
	return ITrueSkyPlugin::Get().GetRenderFloat(name);
}

float ATrueSkySequenceActor::GetFloatAtPosition(FString name,FVector pos)
{
	return ITrueSkyPlugin::Get().GetRenderFloatAtPosition(name,pos);
}

void ATrueSkySequenceActor::SetFloat(FString name, float value )
{
	ITrueSkyPlugin::Get().SetRenderFloat(name,value);
}

int32 ATrueSkySequenceActor::GetInt(FString name ) 
{
	return ITrueSkyPlugin::Get().GetRenderInt(name);
}

void ATrueSkySequenceActor::SetInt(FString name, int32 value )
{
	ITrueSkyPlugin::Get().SetRenderInt(name,value);
}

void ATrueSkySequenceActor::SetBool(FString name, bool value )
{
	ITrueSkyPlugin::Get().SetRenderBool(name,value);
}

float ATrueSkySequenceActor::GetKeyframeFloat(int32 keyframeUid,FString name ) 
{
	return ITrueSkyPlugin::Get().GetKeyframeFloat(keyframeUid,name);
}

void ATrueSkySequenceActor::SetKeyframeFloat(int32 keyframeUid,FString name, float value )
{
	ITrueSkyPlugin::Get().SetKeyframeFloat(keyframeUid,name,value);
}

int32 ATrueSkySequenceActor::GetKeyframeInt(int32 keyframeUid,FString name ) 
{
	return ITrueSkyPlugin::Get().GetKeyframeInt(keyframeUid,name);
}

void ATrueSkySequenceActor::SetKeyframeInt(int32 keyframeUid,FString name, int32 value )
{
	ITrueSkyPlugin::Get().SetKeyframeInt(keyframeUid,name,value);
}

int32 ATrueSkySequenceActor::GetNextModifiableSkyKeyframe() 
{
	return ITrueSkyPlugin::Get().GetRenderInt("NextModifiableSkyKeyframe");
}
	
int32 ATrueSkySequenceActor::GetNextModifiableCloudKeyframe(int32 layer) 
{
	TArray<FVariant> arr;
	arr.Push(FVariant(layer));
	return ITrueSkyPlugin::Get().GetRenderInt("NextModifiableCloudKeyframe",arr);
}

int32 ATrueSkySequenceActor::GetCloudKeyframeByIndex(int32 layer,int32 index)  
{
	TArray<FVariant> arr;
	arr.Push(FVariant(layer));
	arr.Push(FVariant(index));
	return ITrueSkyPlugin::Get().GetRenderInt("Clouds:KeyframeByIndex",arr);
}

int32 ATrueSkySequenceActor::GetNextCloudKeyframeAfterTime(int32 layer,float t)  
{
	TArray<FVariant> arr;
	arr.Push(FVariant(layer));
	arr.Push(FVariant(t));
	return ITrueSkyPlugin::Get().GetRenderInt("Clouds:NextKeyframeAfterTime",arr);
}

int32 ATrueSkySequenceActor::GetPreviousCloudKeyframeBeforeTime(int32 layer,float t)  
{
	TArray<FVariant> arr;
	arr.Push(FVariant(layer));
	arr.Push(FVariant(t));
	return ITrueSkyPlugin::Get().GetRenderInt("Clouds:PreviousKeyframeBeforeTime",arr);
}
	
int32 ATrueSkySequenceActor::GetInterpolatedCloudKeyframe(int32 layer)
{
	TArray<FVariant> arr;
	arr.Push(FVariant(layer)); 
	return ITrueSkyPlugin::Get().GetRenderInt("InterpolatedCloudKeyframe", arr);
}

int32 ATrueSkySequenceActor::GetInterpolatedSkyKeyframe()
{ 
	return ITrueSkyPlugin::Get().GetRenderInt("InterpolatedSkyKeyframe");
}

int32 ATrueSkySequenceActor::GetSkyKeyframeByIndex(int32 index)  
{
	TArray<FVariant> arr;
	arr.Push(FVariant(index));
	return ITrueSkyPlugin::Get().GetRenderInt("Sky:KeyframeByIndex", arr);
}

int32 ATrueSkySequenceActor::GetNextSkyKeyframeAfterTime(float t)  
{
	TArray<FVariant> arr;
	arr.Push(FVariant(t));
	return ITrueSkyPlugin::Get().GetRenderInt("Sky:NextKeyframeAfterTime",arr);
}

int32 ATrueSkySequenceActor::GetPreviousSkyKeyframeBeforeTime(float t)  
{
	TArray<FVariant> arr;
	arr.Push(FVariant(t));
	return ITrueSkyPlugin::Get().GetRenderInt("Sky:PreviousKeyframeBeforeTime",arr);
}

static float m1=1.0f,m2=-1.0f,m3=0,m4=0;
static float c1=0.0f,c2=90.0f,c3=0.0f;

	static float yo=180.0;
	static float a=-1.0f,b=-1.0f,c=-1.0f;
	static FVector dir, u;
	static FTransform worldToSky;
FRotator ATrueSkySequenceActor::GetSunRotation() 
{
	float azimuth	=ITrueSkyPlugin::Get().GetRenderFloat("sky:SunAzimuthDegrees");		// Azimuth in compass degrees from north.
	float elevation	=ITrueSkyPlugin::Get().GetRenderFloat("sky:SunElevationDegrees");
	FRotator sunRotation(c1+m1*elevation,c2+m2*azimuth,c3+m3*elevation+m4*azimuth);
	
	dir= sunRotation.Vector();// = TrueSkyToUEDirection(sunRotation.Vector());
	u = dir;
	ActorCrossThreadProperties *A = NULL;
	A = GetActorCrossThreadProperties();
	if (A)
	{
		worldToSky = A->Transform;
		u=TrueSkyToUEDirection(A->Transform,dir);
//		u = worldToSky.InverseTransformVector(dir);
	}
	float p	=asin(c*u.Z)*180.f/PI_F;
	float y	=atan2(a*u.Y,b*u.X)*180.f/PI_F;
	return FRotator(p,y,0);
}

FRotator ATrueSkySequenceActor::GetMoonRotation() 
{
	float azimuth	=ITrueSkyPlugin::Get().GetRenderFloat("sky:MoonAzimuthDegrees");
	float elevation	=ITrueSkyPlugin::Get().GetRenderFloat("sky:MoonElevationDegrees");
	FRotator moonRotation(c1+m1*elevation,c2+m2*azimuth,c3+m3*elevation+m4*azimuth);
	FVector dir = moonRotation.Vector();// = TrueSkyToUEDirection(sunRotation.Vector());
	FVector u = dir;
	ActorCrossThreadProperties *A = GetActorCrossThreadProperties();
	if (A)
	{
		FTransform worldToSky = A->Transform;
		u = worldToSky.InverseTransformVector(dir);
	}
	float p = asin(c*u.Z)*180.f / PI_F;
	float y = atan2(a*u.Y, b*u.X)*180.f / PI_F;
	return FRotator(p, y, 0);
}

void ATrueSkySequenceActor::SetSunRotation(FRotator r)
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	FVector dir		=UEToTrueSkyDirection(A->Transform,r.Vector());
	float elevation	=-r.Pitch*PI_F/180.0f;			// -ve because we want the angle TO the light, not from it.
	float azimuth	=(r.Yaw+yo)*PI_F/180.0f;		// yaw 0 = east, 90=north
	elevation		=asin(-dir.Z);
	azimuth			=atan2(-dir.Y,-dir.X);
	TArray<FVariant> arr;
	arr.Push(FVariant(azimuth));
	arr.Push(FVariant(elevation));
	ITrueSkyPlugin::Get().SetRender("override:sundirection",arr);
}

void ATrueSkySequenceActor::SetMoonRotation(FRotator r)
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	FVector dir=UEToTrueSkyDirection(A->Transform,r.Vector());
	float elevation	=-r.Pitch*PI_F/180.0f;
	float azimuth	=(r.Yaw+yo)*PI_F/180.0f;		// yaw 0 = east, 90=north
	elevation		=asin(-dir.Z);
	azimuth			=atan2(-dir.Y,-dir.X);
	TArray<FVariant> arr;
	arr.Push(FVariant(azimuth));
	arr.Push(FVariant(elevation));
	ITrueSkyPlugin::Get().SetRender("override:moondirection",arr);
}

float ATrueSkySequenceActor::CloudLineTest(int32 queryId,FVector StartPos,FVector EndPos) 
{
	return ITrueSkyPlugin::Get().CloudLineTest(queryId,StartPos,EndPos);
}

float ATrueSkySequenceActor::CloudPointTest(int32 queryId,FVector pos) 
{
	return ITrueSkyPlugin::Get().GetCloudinessAtPosition(queryId,pos);
}
float ATrueSkySequenceActor::GetCloudShadowAtPosition(int32 queryId,FVector pos) 
{
	ITrueSkyPlugin::VolumeQueryResult res= ITrueSkyPlugin::Get().GetStateAtPosition(queryId,pos);
	return (1 - res.direct_light);
}
float ATrueSkySequenceActor::GetRainAtPosition(int32 queryId,FVector pos) 
{
	ITrueSkyPlugin::VolumeQueryResult res= ITrueSkyPlugin::Get().GetStateAtPosition(queryId,pos);
	return res.precipitation;
}

FLinearColor ATrueSkySequenceActor::GetSunColor() 
{
	float r	=ITrueSkyPlugin::Get().GetRenderFloat("SunIrradianceRed");
	float g	=ITrueSkyPlugin::Get().GetRenderFloat("SunIrradianceGreen");
	float B	=ITrueSkyPlugin::Get().GetRenderFloat("SunIrradianceBlue");
	float m=std::max(std::max(std::max(r,g),B),1.0f);
	r/=m;
	g/=m;
	B/=m;
	return FLinearColor( r, g, B );
}

FLinearColor ATrueSkySequenceActor::GetMoonColor() 
{
	float r	=ITrueSkyPlugin::Get().GetRenderFloat("MoonIrradianceRed");
	float g	=ITrueSkyPlugin::Get().GetRenderFloat("MoonIrradianceGreen");
	float B	=ITrueSkyPlugin::Get().GetRenderFloat("MoonIrradianceBlue");
	float m=std::max(std::max(std::max(r,g),B),1.0f);
	r/=m;
	g/=m;
	B/=m;
	return FLinearColor( r, g, B );
}

float ATrueSkySequenceActor::GetSunIntensity() 
{
	float r	=ITrueSkyPlugin::Get().GetRenderFloat("SunIrradianceRed");
	float g	=ITrueSkyPlugin::Get().GetRenderFloat("SunIrradianceGreen");
	float B	=ITrueSkyPlugin::Get().GetRenderFloat("SunIrradianceBlue");
	float m=std::max(std::max(std::max(r,g),B),1.0f);
	return m;
}

float ATrueSkySequenceActor::GetMoonIntensity() 
{
	float r	=ITrueSkyPlugin::Get().GetRenderFloat("MoonIrradianceRed");
	float g	=ITrueSkyPlugin::Get().GetRenderFloat("MoonIrradianceGreen");
	float B	=ITrueSkyPlugin::Get().GetRenderFloat("MoonIrradianceBlue");
	float m=std::max(std::max(std::max(r,g),B),1.0f);
	return m;
}

void ATrueSkySequenceActor::SetPointLightSource(int id,FLinearColor lc,float Intensity,FVector pos,float min_radius,float max_radius) 
{
	ITrueSkyPlugin::Get().SetPointLight(id,lc*Intensity,pos, min_radius, max_radius);
}

void ATrueSkySequenceActor::SetPointLight(APointLight *source) 
{
	int id=(int)(int64_t)(source);
	FLinearColor lc=source->PointLightComponent->LightColor;
	lc*=source->PointLightComponent->Intensity;
	FVector pos=source->GetActorLocation();
	float min_radius=source->PointLightComponent->SourceRadius;
	float max_radius=source->PointLightComponent->AttenuationRadius;
	ITrueSkyPlugin::Get().SetPointLight(id,lc,pos, min_radius, max_radius);
}

void ATrueSkySequenceActor::TransferProperties()
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	if(!A)
		return;
	if (!IsActiveActor())
	{
		if(!IsAnyActorActive())
			A->activeSequence			=nullptr;
		return;
	}
	SimulVersion simulVersion		=ITrueSkyPlugin::Get().GetVersion();
	bSimulVersion4_1				=(simulVersion>SIMUL_4_0);
	A->InterpolationMode			=(uint8_t)InterpolationMode;
	A->InterpolationTimeSeconds		=InterpolationTimeSeconds;
	A->PrecipitationOptions			=(VelocityStreaks?(uint8_t)PrecipitationOptionsEnum::VelocityStreaks:0)
										|(SimulationTime?(uint8_t)PrecipitationOptionsEnum::SimulationTime:0);//.GetValue();
	A->ShareBuffersForVR			=ShareBuffersForVR;
	A->Destroyed					=false;
	A->Visible						=Visible;
	A->MetresPerUnit				=MetresPerUnit;
	A->Brightness					=Brightness;
	A->SimpleCloudShadowing			=SimpleCloudShadowing;
	UWorld *world = GetWorld();
	A->Playing=true;
	if(A->activeSequence!=ActiveSequence)
	{
		A->activeSequence				=ActiveSequence;
		if (!world|| world->WorldType == EWorldType::Editor)
		{
			A->Reset=true;
			A->Playing=false;
		}
	}
	A->SimpleCloudShadowSharpness	=SimpleCloudShadowSharpness;
	A->CloudThresholdDistanceKm		=CloudThresholdDistanceKm;
	A->DownscaleFactor				=DownscaleFactor;
	A->MaximumResolution			=MaximumResolution;
	A->DepthSamplingPixelRange		=DepthSamplingPixelRange;
	A->Amortization					=Amortization;
	A->AtmosphericsAmortization		=AtmosphericsAmortization;
	A->MoonTexture					=MoonTexture;
	A->CosmicBackgroundTexture		=CosmicBackgroundTexture;
	A->LossRT						=LossRT;
	A->CloudVisibilityRT			=CloudVisibilityRT;
	A->CloudShadowRT				=CloudShadowRT;
	A->InscatterRT					=InscatterRT;
	A->RainCubemap					=RainCubemap;
	A->Time							=Time;
	A->MaxSunRadiance				=MaxSunRadiance;
	// Note: This seems to be necessary, it's not clear why:
	if(RootComponent)
		RootComponent->UpdateComponentToWorld();
	A->Transform					=GetTransform();
	A->DepthBlending				=DepthBlending;
	A->MinimumStarPixelSize			=MinimumStarPixelSize;
	for(int i=0;i<4;i++)
	{
		LightningBolt *L=&(A->lightningBolts[i]);
		if(L&&L->id!=0&&L->id!=latest_thunderbolt_id)
		{
			FVector pos(L->endpos[0],L->endpos[1],L->endpos[2]);
			float magnitude=L->brightness;
			FVector colour(L->colour[0],L->colour[1],L->colour[2]);
			latest_thunderbolt_id=L->id;
			if(ThunderSounds.Num())
			{
				for( FConstPlayerControllerIterator Iterator = world->GetPlayerControllerIterator(); Iterator; ++Iterator )
				{
					FVector listenPos,frontDir,rightDir;
					Iterator->Get()->GetAudioListenerPosition(listenPos,frontDir,rightDir);
					FVector offset		=listenPos-pos;
				
					FTimerHandle UnusedHandle;
					float dist			=offset.Size()*A->MetresPerUnit;
					static float vsound	=3400.29f;
					float delaySeconds	=dist/vsound;
					FTimerDelegate soundDelegate = FTimerDelegate::CreateUObject( this, &ATrueSkySequenceActor::PlayThunder, pos );
					GetWorldTimerManager().SetTimer(UnusedHandle,soundDelegate,delaySeconds,false);
				}
			}
		}
	}
}

void ATrueSkySequenceActor::PlayThunder(FVector pos)
{
	USoundBase *ThunderSound=ThunderSounds[FMath::Rand()%ThunderSounds.Num()];
	if(ThunderSound)
		UGameplayStatics::PlaySoundAtLocation(this,ThunderSound,pos,FMath::RandRange(0.5f,1.5f),FMath::RandRange(0.7f,1.3f),0.0f,ThunderAttenuation);
}


float ATrueSkySequenceActor::GetMetresPerUnit()
{
	ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
	return A->MetresPerUnit;
}

#ifndef UE_LOG4_ONCE
#define UE_LOG4_ONCE(a,b,c,d) {static bool done=false; if(!done) {UE_LOG(a,b,c,d);done=true;}}
#endif

bool ATrueSkySequenceActor::IsActiveActor()
{
	UWorld *world = GetWorld();
	if(HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject))
		return false;
	// WorldType is often not yet initialized here, even though we're calling from PostLoad.
	if (!world||world->WorldType == EWorldType::Editor||world->WorldType==EWorldType::Inactive)
		return ActiveInEditor;
	return (this == worldCurrentActiveActors[world]);
}

bool ATrueSkySequenceActor::IsAnyActorActive() 
{
	UWorld *world = GetWorld();
	if(!HasAnyFlags(RF_Transient|RF_ClassDefaultObject|RF_ArchetypeObject))
		if (!world||world->WorldType == EWorldType::Editor||world->WorldType==EWorldType::Inactive)
			return true;
	if(worldCurrentActiveActors[world]!=nullptr)
		return true;
	return false;
}


FBox ATrueSkySequenceActor::GetComponentsBoundingBox(bool bNonColliding) const
{
	FBox Box(0);

	for (const UActorComponent* ActorComponent : GetComponents())
	{
		const UBoxComponent* PrimComp = Cast<const UBoxComponent>(ActorComponent);
		if (PrimComp)
		{
			// Only use collidable components to find collision bounding box.
			if (PrimComp->IsRegistered() && (bNonColliding || PrimComp->IsCollisionEnabled()))
			{
				Box += PrimComp->Bounds.GetBox();
			}
		}
	}

	return Box;
}

void ATrueSkySequenceActor::FillMaterialParameters()
{
	static float mix = 0.05;
	for(TObjectIterator<UMaterialParameterCollection> Itr; Itr; ++Itr)
	{
		FString str=(Itr->GetName());
		UMaterialParameterCollection * mpc= *Itr;
		if(mpc&&str==FString(L"trueSkyMaterialParameters"))
		{
			ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
			float MaxAtmosphericDistanceKm=GetFloat("sky:MaxDistanceKm");
			FVector CloudShadowOriginKm(0,0,0);
			FVector CloudShadowScaleKm(0,0,0);
			CloudShadowOriginKm.X=GetFloat("CloudShadowOriginKm.x");
			CloudShadowOriginKm.Y=GetFloat("CloudShadowOriginKm.y");
			CloudShadowOriginKm.Z=GetFloat("CloudShadowOriginKm.z");
			CloudShadowScaleKm.X=GetFloat("CloudShadowScaleKm.x");
			CloudShadowScaleKm.Y=GetFloat("CloudShadowScaleKm.y");
			CloudShadowScaleKm.Z=GetFloat("CloudShadowScaleKm.z");
			FVector pos=A->Transform.GetTranslation();
			wetness*=1.0-mix;
			wetness+=mix*GetFloatAtPosition("precipitation",pos);
			//ActorCrossThreadProperties *A	=GetActorCrossThreadProperties();
			UKismetMaterialLibrary::SetScalarParameterValue(this,mpc,"MaxAtmosphericDistance",MaxAtmosphericDistanceKm*1000.0f/A->MetresPerUnit);
			UKismetMaterialLibrary::SetVectorParameterValue(this,mpc,"CloudShadowOrigin",CloudShadowOriginKm*1000.0f/A->MetresPerUnit);
			UKismetMaterialLibrary::SetVectorParameterValue(this,mpc,"CloudShadowScale",CloudShadowScaleKm*1000.0f/A->MetresPerUnit);
			UKismetMaterialLibrary::SetScalarParameterValue(this,mpc,"LocalWetness",wetness);
		}
	}
}

void ATrueSkySequenceActor::TickActor(float DeltaTime,enum ELevelTick TickType,FActorTickFunction& ThisTickFunction)
{
	UWorld *world = GetWorld();
	FillMaterialParameters();
	// We DO NOT accept ticks from actors in Editor worlds.
	// For some reason, actors start ticking when you Play in Editor, the ones in the Editor and well as the ones in the PIE world.
	// This results in duplicate actors competing.
	if (!world || world->WorldType == EWorldType::Editor)
		return;
	// Find out which trueSKY actor should be active. We should only do this once per frame, so this is inefficient at present.

	ATrueSkySequenceActor *CurrentActiveActor = NULL;
	ATrueSkySequenceActor* GlobalActor = NULL;
	APawn* playerPawn = UGameplayStatics::GetPlayerPawn(world, 0);
	FVector pos(0, 0, 0);
	if (playerPawn)
		pos = playerPawn->GetActorLocation();
	else if (GEngine)
	{
		APlayerController * pc = GEngine->GetFirstLocalPlayerController(world);
		if (pc)
		{
			if (pc->PlayerCameraManager)
			{
				pos = pc->PlayerCameraManager->GetCameraLocation();
			}
		}
	}
	for (TActorIterator<ATrueSkySequenceActor> Itr(world); Itr; ++Itr)
	{
		ATrueSkySequenceActor* SequenceActor = *Itr;
		if (!SequenceActor->Visible)
			continue;
		FBox box = SequenceActor->GetComponentsBoundingBox(true);
		if (box.GetSize().IsZero())
			GlobalActor = SequenceActor;
		else if (box.IsInside(pos))
			CurrentActiveActor = SequenceActor;
	}
	if (CurrentActiveActor == nullptr)
		CurrentActiveActor = GlobalActor;
	
	worldCurrentActiveActors[world] = CurrentActiveActor;
	if (!IsActiveActor())
		return;
	if(!world->IsPaused())
		Time+=DeltaTime;
	TransferProperties();
	UpdateVolumes();
}

#if WITH_EDITOR
void ATrueSkySequenceActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RootComponent)
		RootComponent->UpdateComponentToWorld();
	TransferProperties();
	UpdateVolumes();
	// If this is the active Actor, deactivate the others:
	if (ActiveInEditor)
	{
		UWorld *world = GetWorld();
		if (!world||world->WorldType!= EWorldType::Editor)
		{
			return;
		}
		for (TActorIterator<ATrueSkySequenceActor> Itr(world); Itr; ++Itr)
		{
			ATrueSkySequenceActor* SequenceActor = *Itr;
			if (SequenceActor == this)
				continue;
			SequenceActor->ActiveInEditor = false;
		}
		worldCurrentActiveActors[world] = this;
		// Force instant, not gradual change, if the change was from editing:
		ITrueSkyPlugin::Get().TriggerAction("Reset");
	}
}
#endif

//UAudioComponent* ATrueSkySequenceActor::GetRainAudioComponent() const { return RainAudioComponent; }
//UAudioComponent* ATrueSkySequenceActor::GetThunderAudioComponent() const { return ThunderAudioComponent; }
