#include "TrueSkyPluginPrivatePCH.h"
#include "TrueSkyLightComponent.h"
#if WITH_EDITOR
#include "ObjectEditorUtils.h"
#endif
#include "Engine/SkyLight.h"
#include "MessageLog.h"
#include "UObjectToken.h"
#include "Net/UnrealNetwork.h"
#include "MapErrors.h"
#include "ComponentInstanceDataCache.h"
#include "ShaderCompiler.h"
#include "Engine/TextureCube.h"		// So that UTextureCube* can convert to UTexture* when calling FSceneInterface::UpdateSkyCaptureContents
#include "Components/SkyLightComponent.h"
#include "Engine/TextureRenderTargetCube.h"

#define LOCTEXT_NAMESPACE "TrueSkyLightComponent"

extern ENGINE_API int32 GReflectionCaptureSize;

TArray<UTrueSkyLightComponent*> UTrueSkyLightComponent::TrueSkyLightComponents;

UTrueSkyLightComponent::UTrueSkyLightComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	if (!IsRunningCommandlet())
	{
		static ConstructorHelpers::FObjectFinder<UTexture2D> StaticTexture(TEXT("/Engine/EditorResources/LightIcons/SkyLight"));
		StaticEditorTexture = StaticTexture.Object;
		StaticEditorTextureScale = 1.0f;
		DynamicEditorTexture = StaticTexture.Object;
		DynamicEditorTextureScale = 1.0f;
	}
#endif

	Brightness_DEPRECATED = 1;
	Intensity = 1;
	IndirectLightingIntensity = 1.0f;
	SkyDistanceThreshold = 150000;
	Mobility = EComponentMobility::Stationary;
	bLowerHemisphereIsBlack = true;
	bSavedConstructionScriptValuesValid = true;
	bHasEverCaptured = false;
	OcclusionMaxDistance = 1000;
	MinOcclusion = 0;
	OcclusionTint = FColor::Black;
	bIsInitialized=false;
}


void UTrueSkyLightComponent::CreateRenderState_Concurrent()
{
	Super::CreateRenderState_Concurrent();

	bool bHidden = false;
#if WITH_EDITORONLY_DATA
	bHidden = GetOwner() ? GetOwner()->bHiddenEdLevel : false;
#endif // WITH_EDITORONLY_DATA

	if(!ShouldComponentAddToScene())
	{
		bHidden = true;
	}

	const bool bIsValid = SourceType != SLS_SpecifiedCubemap || Cubemap != NULL;

	if (bAffectsWorld && bVisible && !bHidden && bIsValid)
	{
		// From USkyLightComponent::UpdateSkyCaptureContentsArray.
		// As we'll never do a capture, we must create the processed texture here.
/*		if (!ProcessedSkyTexture)
		{
			ProcessedSkyTexture = new FSkyTextureCubeResource();
			ProcessedSkyTexture->SetupParameters(GReflectionCaptureSize, FMath::CeilLogTwo(GReflectionCaptureSize) + 1, PF_FloatRGBA);
			BeginInitResource(ProcessedSkyTexture);
			MarkRenderStateDirty();
		}*/

		// Create the light's scene proxy. ProcessedSkyTexture may be NULL still here, so we can't use CreateSceneProxy()
		SceneProxy = new FSkyLightSceneProxy(this);
		bIsInitialized=false;
		if (SceneProxy&&GetWorld())
		{
			// Add the light to the scene.
			GetWorld()->Scene->SetSkyLight(SceneProxy);
			const int nums=SceneProxy->IrradianceEnvironmentMap.R.NumSIMDVectors*SceneProxy->IrradianceEnvironmentMap.R.NumComponentsPerSIMDVector;
			for(int i=0;i<nums;i++)
			{
				SceneProxy->IrradianceEnvironmentMap.R.V[i]=0.0f;
				SceneProxy->IrradianceEnvironmentMap.G.V[i]=0.0f;
				SceneProxy->IrradianceEnvironmentMap.B.V[i]=0.0f;
			}
		}
	}
}

void UTrueSkyLightComponent::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Enqueue an update by default, so that newly placed components will get an update
		// PostLoad will undo this for components loaded from disk
		TrueSkyLightComponents.AddUnique(this);
	}
	// Skip over USkyLightComponent::PostInitProperties to avoid putting this skylight in the update list
	// as that would trash the calculated values.
	//ULightComponentBase::PostInitProperties();
	// BUT: That means the proxy won't get created. We need to do at least ONE capture to make sure it will be.
	USkyLightComponent::PostInitProperties();
}

void UTrueSkyLightComponent::PostLoad()
{
	Super::PostLoad();

	// All components are queued for update on creation by default, remove if needed
	if (!bVisible || HasAnyFlags(RF_ClassDefaultObject))
	{
		TrueSkyLightComponents.Remove(this);
	}
}


/** 
* This is called when property is modified by InterpPropertyTracks
*
* @param PropertyThatChanged	Property that changed
*/
void UTrueSkyLightComponent::PostInterpChange(UProperty* PropertyThatChanged)
{
	static FName LightColorName(TEXT("LightColor"));
	static FName IntensityName(TEXT("Intensity"));
	static FName IndirectLightingIntensityName(TEXT("IndirectLightingIntensity"));

	FName PropertyName = PropertyThatChanged->GetFName();
	if (PropertyName == LightColorName
		|| PropertyName == IntensityName
		|| PropertyName == IndirectLightingIntensityName)
	{
		UpdateLimitedRenderingStateFast();
	}
	else
	{
		Super::PostInterpChange(PropertyThatChanged);
	}
}

void UTrueSkyLightComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();

	if (SceneProxy&&GetWorld())
	{
		GetWorld()->Scene->DisableSkyLight(SceneProxy);

		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			FDestroySkyLightCommand,
			FSkyLightSceneProxy*,LightSceneProxy,SceneProxy,
		{
			delete LightSceneProxy;
		});

		SceneProxy = NULL;
	}
}

#if WITH_EDITOR
void UTrueSkyLightComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Skip over USkyLightComponent::PostEditChangeProperty to avoid trashing values.
	ULightComponentBase::PostEditChangeProperty(PropertyChangedEvent);
}

bool UTrueSkyLightComponent::CanEditChange(const UProperty* InProperty) const
{
	if (InProperty)
	{
		FString PropertyName = InProperty->GetName();

		if (FCString::Strcmp(*PropertyName, TEXT("Cubemap")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("SourceCubemapAngle")) == 0)
		{
			return SourceType == SLS_SpecifiedCubemap;
		}

		if (FCString::Strcmp(*PropertyName, TEXT("Contrast")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("OcclusionMaxDistance")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("MinOcclusion")) == 0
			|| FCString::Strcmp(*PropertyName, TEXT("OcclusionTint")) == 0)
		{
			static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GenerateMeshDistanceFields"));
			return Mobility == EComponentMobility::Movable && CastShadows && CVar->GetValueOnGameThread() != 0;
		}
	}

	return Super::CanEditChange(InProperty);
}

void UTrueSkyLightComponent::CheckForErrors()
{
	AActor* Owner = GetOwner();

	if (Owner && bVisible && bAffectsWorld)
	{
		UWorld* ThisWorld = Owner->GetWorld();
		bool bMultipleFound = false;

		if (ThisWorld)
		{
			for (TObjectIterator<UTrueSkyLightComponent> ComponentIt; ComponentIt; ++ComponentIt)
			{
				UTrueSkyLightComponent* Component = *ComponentIt;

				if (Component != this 
					&& !Component->IsPendingKill()
					&& Component->bVisible
					&& Component->bAffectsWorld
					&& Component->GetOwner() 
					&& ThisWorld->ContainsActor(Component->GetOwner())
					&& !Component->GetOwner()->IsPendingKill())
				{
					bMultipleFound = true;
					break;
				}
			}
		}

		if (bMultipleFound)
		{
			FMessageLog("MapCheck").Error()
				->AddToken(FUObjectToken::Create(Owner))
				->AddToken(FTextToken::Create(LOCTEXT( "MapCheck_Message_MultipleSkyLights", "Multiple sky lights are active, only one can be enabled per world." )))
				->AddToken(FMapErrorToken::Create(FMapErrors::MultipleSkyLights));
		}
	}
}

#endif // WITH_EDITOR

void UTrueSkyLightComponent::BeginDestroy()
{
	Super::BeginDestroy();
	TrueSkyLightComponents.Remove(this);
}

bool UTrueSkyLightComponent::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy();
}


FActorComponentInstanceData* UTrueSkyLightComponent::GetComponentInstanceData() const
{
	return USkyLightComponent::GetComponentInstanceData();
}


void UTrueSkyLightComponent::SetVisibility(bool bNewVisibility, bool bPropagateToChildren)
{
	const bool bOldWasVisible = bVisible;

	ULightComponentBase::SetVisibility(bNewVisibility, bPropagateToChildren);
}

void UTrueSkyLightComponent::RecaptureSky()
{
// don't	SetCaptureIsDirty();
}

void UTrueSkyLightComponent::SetHasUpdated()
{
	bHasEverCaptured = true;
//	MarkRenderStateDirty();
}

#undef LOCTEXT_NAMESPACE