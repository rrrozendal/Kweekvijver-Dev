// Copyright Simul Software Ltd


#pragma once
#include "TrueSkyLightComponent.generated.h"

UCLASS(Blueprintable, ClassGroup=Lights, HideCategories=(Trigger,Activation,"Components|Activation",Physics), meta=(BlueprintSpawnableComponent))
class UTrueSkyLightComponent : public USkyLightComponent
{
	GENERATED_UCLASS_BODY()
	//~ Begin UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostInterpChange(UProperty* PropertyThatChanged) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void CheckForErrors() override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	//~ End UObject Interface

	bool IsInitialized() const
	{
		return bIsInitialized;
	}
	void SetInitialized(bool b)
	{
		bIsInitialized=b;
	}

	virtual FActorComponentInstanceData* GetComponentInstanceData() const override;

	virtual void SetVisibility(bool bNewVisibility, bool bPropagateToChildren=false) override;

	/** 
	 * Recaptures the scene for the skylight. 
	 * This is useful for making sure the sky light is up to date after changing something in the world that it would capture.
	 * Warning: this is very costly and will definitely cause a hitch.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Components|SkyLight")
	void RecaptureSky();

	static TArray<UTrueSkyLightComponent*> &GetTrueSkyLightComponents()
	{
		return TrueSkyLightComponents;
	}
	FSkyLightSceneProxy *GetSkyLightSceneProxy()
	{
		return SceneProxy;
	}

	void SetHasUpdated();
protected:
	static TArray<UTrueSkyLightComponent*> TrueSkyLightComponents;
	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
	//~ Begin UActorComponent Interface

	friend class FSkyLightSceneProxy;
	bool bIsInitialized;
};



