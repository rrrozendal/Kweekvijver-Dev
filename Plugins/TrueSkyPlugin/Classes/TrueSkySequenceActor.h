#pragma once

#include "TrueSkySequenceActor.generated.h"

UENUM()
enum class EInterpolationMode : uint8
{
	FixedNumber=0,
	RealTime=1
};

UENUM()
 enum class PrecipitationOptionsEnum : uint8
 {
     None=0
	 ,VelocityStreaks	=0x1
	 ,SimulationTime	=0x2
 };

/*
example
 enum class ETestType : uint8
 {
     None                = 0            UMETA(DisplayName="None"),
     Left                = 0x1        UMETA(DisplayName=" Left"),
     Right                = 0x2        UMETA(DisplayName=" Right"),
 };
 
 ENUM_CLASS_FLAGS(ETestType);

  UPROPERTY()
 TEnumAsByte<EnumName> VarName;
*/
//hideCategories=(Actor, Advanced, Display, Object, Attachment, Movement, Collision, Rendering, Input), 
UCLASS(MinimalAPI,Blueprintable)
class ATrueSkySequenceActor : public AActor
{
	GENERATED_UCLASS_BODY()
	~ATrueSkySequenceActor();
	
	/** The method to use for interpolation. Used from trueSKY 4.1 onwards.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=Interpolation)
	EInterpolationMode InterpolationMode;
	
	/** The time for real time interpolation in seconds. Used from trueSKY 4.1 onwards.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=Interpolation)
	float InterpolationTimeSeconds;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=Precipitation)
	bool VelocityStreaks;
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=Precipitation)
	bool SimulationTime;
	/** Returns the spectral radiance as a multiplier for colour values output by trueSKY.
		For example, a pixel value of 1.0, with a reference radiance of 2.0, would represent 2 watts per square metre per steradian. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static float GetReferenceSpectralRadiance();

	UFUNCTION(BlueprintCallable,Category = TrueSky)
	static int32 GetLightning(FVector &start,FVector &end,float &magnitude,FVector &colour);

	UFUNCTION( BlueprintCallable,Category = TrueSky)
	static int32 SpawnLightning(FVector pos1,FVector pos2,float magnitude,FVector colour);

	void Destroyed() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static FString GetProfilingText(int32 cpu_level,int32 gpu_level);

	/** Sets the time value for trueSKY. By default, 0=midnight, 0.5=midday, 1.0= the following midnight, etc.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetTime( float value );
	
	/** Returns the time value for trueSKY. By default, 0=midnight, 0.5=midday, 1.0= the following midnight, etc.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static float GetTime();

	/** Returns the named floating-point property.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static float GetFloat(FString name );
	
	/** Returns the named floating-point property.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	float GetFloatAtPosition(FString name,FVector pos);

	/** Set the named floating-point property.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetFloat(FString name, float value );
	
	/** Returns the named integer property.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetInt(FString name ) ;
	
	/** Set the named integer property.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetInt(FString name, int32 value);
	
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetBool(FString name, bool value );

	/** Returns the named keyframe property for the keyframe identified.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static float GetKeyframeFloat(int32 keyframeUid,FString name );
	
	/** Set the named keyframe property for the keyframe identified.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetKeyframeFloat(int32 keyframeUid,FString name, float value );
	
	/** Returns the named integer keyframe property for the keyframe identified.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetKeyframeInt(int32 keyframeUid,FString name );
	
	/** Set the named integer keyframe property for the keyframe identified.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetKeyframeInt(int32 keyframeUid,FString name, int32 value );
	
	/** Returns the calculated sun direction.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static FRotator GetSunRotation();

	/** Returns the calculated moon direction.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static FRotator GetMoonRotation();
	
	/** Override the sun direction.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetSunRotation(FRotator r);
	
	/** Override the moon direction.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetMoonRotation(FRotator r);
	
	/** Returns the amount of cloud at the given position.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static float CloudLineTest(int32 queryId,FVector StartPos,FVector EndPos);

	/** Returns the amount of cloud at the given position.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static float CloudPointTest(int32 QueryId,FVector pos);

	/** Returns the cloud shadow at the given position, from 0 to 1.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static float GetCloudShadowAtPosition(int32 QueryId,FVector pos);

	/** Returns the rainfall at the given position, from 0 to 1.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static float GetRainAtPosition(int32 QueryId,FVector pos);

	/** Returns the calculated sun colour in irradiance units, divided by intensity.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static FLinearColor GetSunColor();
	
	/** Returns the Sequence Actor singleton.*/
	UFUNCTION(BlueprintCallable,BlueprintPure,Category=TrueSky)
	static float GetMetresPerUnit();

	/** Returns the calculated moon intensity in irradiance units.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static float GetSunIntensity();
	
	/** The maximum sun brightness to be rendered by trueSKY, in radiance units.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky)
	float MaxSunRadiance;

	/** Returns the calculated moon intensity in irradiance units.*/
	UFUNCTION(BlueprintCallable, BlueprintPure,Category=TrueSky)
	static float GetMoonIntensity();

	/** Returns the calculated moon colour in irradiance units, divided by intensity.*/
	UFUNCTION(BlueprintCallable, BlueprintPure, Category=TrueSky)
	static FLinearColor GetMoonColor();

	/** Illuminate the clouds with the specified values from position pos.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetPointLightSource(int32 id,FLinearColor lightColour,float Intensity,FVector pos,float minRadius,float maxRadius);
	
	/** Illuminate the clouds with the specified point light.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static void SetPointLight(APointLight *source);

	/** When multiple trueSKY Sequence Actors are present in a level, the active one is selected using this checkbox.*/
	UPROPERTY(EditAnywhere,Category = TrueSky)
	bool ActiveInEditor;

	/** What is the current active sequence for this actor? */
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky)
	class UTrueSkySequenceAsset* ActiveSequence;
	
	/** Get an identifier for the next sky keyframe that can be altered without requiring a recalculation of the tables.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetNextModifiableSkyKeyframe();
	
	/** Get an identifier for the next cloud keyframe that can be altered without requiring a recalculation of the 3D textures.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetNextModifiableCloudKeyframe(int32 layer);

	/** Get an identifier for the cloud keyframe at the specified index. Returns zero if there is none at that index (e.g. you have gone past the end of the list).*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetCloudKeyframeByIndex(int32 layer,int32 index);

	/** Get an identifier for the next cloud keyframe at or after the specified time.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetNextCloudKeyframeAfterTime(int32 layer,float t);

	/** Get an identifier for the last cloud keyframe before the specified time.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetPreviousCloudKeyframeBeforeTime(int32 layer,float t);
	
	/** Get an identifier for the sky keyframe at the specified index. Returns zero if there is none at that index (e.g. you have gone past the end of the list).*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetSkyKeyframeByIndex(int32 index);

	/** Get an identifier for the next cloud keyframe at or after the specified time.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetNextSkyKeyframeAfterTime(float t);

	/** Get an identifier for the last sky keyframe before the specified time.*/
	UFUNCTION(BlueprintCallable, Category=TrueSky)
	static int32 GetPreviousSkyKeyframeBeforeTime(float t);

	/** Get an indentifier for the interpolated cloud keyframe */
	UFUNCTION(BlueprintCallable, Category = TrueSky)
	static int32 GetInterpolatedCloudKeyframe(int32 layer);

	/** Get an indentifier for the interpolated sky keyframe */
	UFUNCTION(BlueprintCallable, Category = TrueSky)
		static int32 GetInterpolatedSkyKeyframe();

	//UPROPERTY(EditAnywhere, Category=TrueSky)
	//UTextureRenderTarget2D* CloudShadowRenderTarget;
	/** The texture to draw for the moon.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSkyTextures)
	UTexture2D* MoonTexture;
	
	/** The texture to draw for the cosmic background - e.g. the Milky Way.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = TrueSkyTextures)
	UTexture2D* CosmicBackgroundTexture;

	/** If set, trueSKY render rain and snow particles to this target - use it for compositing with the.*/
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrueSky)
	//UTextureRenderTarget2D* RainOverlayRT;

	/** If set, trueSKY will use this cubemap to light the rain, otherwise a TrueSkyLight will be used.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrueSkyTextures)
	UTexture* RainCubemap;

	/** The render texture to fill in with atmospheric inscatter values.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = TrueSkyTextures)
	UTextureRenderTarget2D* InscatterRT;

	/** The render texture to fill in with atmospheric loss values.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = TrueSkyTextures)
	UTextureRenderTarget2D* LossRT;
	
	/** The render texture to fill in with the cloud shadow.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = TrueSkyTextures, meta=(EditCondition="bSimulVersion4_1"))
	UTextureRenderTarget2D* CloudShadowRT;

	/** The render texture to fill in with atmospheric loss values.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = TrueSkyTextures)
	UTextureRenderTarget2D* CloudVisibilityRT;
	/** A multiplier for brightness of the trueSKY environment, 1.0 by default.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky,meta=(ClampMin = "0.1", ClampMax = "10.0"))
	float Brightness;
	
	/** Tells trueSKY how many metres there are in a single UE4 distance unit. Typically 0.1 (10cm per unit).*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky,meta=(ClampMin = "0.001", ClampMax = "1000.0"))
	float MetresPerUnit;
	
	/** Tells trueSKY whether and to what extent to apply cloud shadows to the scene in post-processing.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky,meta=(ClampMin = "0.0", ClampMax = "1.0"))
	float SimpleCloudShadowing;
	
	/** Tells trueSKY how sharp to make the shadow boundaries.*/
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky,meta=(ClampMin = "0.0", ClampMax = "1.0"))
	float SimpleCloudShadowSharpness;

	/** A heuristic distance to discard near depths from depth interpolation, improving accuracy of upscaling.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrueSky, meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float CloudThresholdDistanceKm;
	
	/** Deprecated. Instead, use MaximumResolution. Tells trueSKY how much to downscale resolution for cloud rendering. The scaling is 2^downscaleFactor.*/
	UPROPERTY(Config=Engine,EditAnywhere,BlueprintReadWrite, Category=TrueSky,meta=(EditCondition="!bSimulVersion4_1",ClampMin = "1", ClampMax = "4"))
	int32 DownscaleFactor;

	/** The largest cubemap resolution to be used for cloud rendering. Typically 1/4 of the screen width, larger values are more expensive in time and memory.*/
	UPROPERTY(Config=Engine,EditAnywhere,BlueprintReadWrite, Category=TrueSky, meta=(EditCondition="bSimulVersion4_1",ClampMin = "16", ClampMax = "2048"))
	int32 MaximumResolution;

	/** The size, in pixels, of the sampling area in the full-resolution depth buffer, which is used to find near/far depths.*/
	UPROPERTY(Config=Engine,EditAnywhere,BlueprintReadWrite, Category=TrueSky, meta=(EditCondition="bSimulVersion4_1",ClampMin = "0.0", ClampMax = "4.0"))
	float DepthSamplingPixelRange;

	/** Tells trueSKY how to spread the cost of rendering over frames. For 1, all pixels are drawn every frame, for amortization 2, it's 2x2, etc.*/
	UPROPERTY(Config=Engine,EditAnywhere,BlueprintReadWrite, Category=TrueSky,meta=(ClampMin = "1", ClampMax = "8"))
	int32 Amortization;
	
	/** Tells trueSKY how to spread the cost of rendering atmospherics over frames.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrueSky, meta = (ClampMin = "1", ClampMax = "8"))
	int32 AtmosphericsAmortization;

	/** Tells trueSKY whether to blend clouds with scenery, or to draw them in front/behind depending on altitude.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrueSky)
	bool DepthBlending;

	/** The smallest size stars can be drawn. If zero they are drawn as points, otherwise as quads. Use this to compensate for artifacts caused by antialiasing.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrueSky, meta=(EditCondition="bSimulVersion4_1"))
	float MinimumStarPixelSize;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky)
	bool ShareBuffersForVR;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category=TrueSky)
	bool Visible;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	class USoundBase* RainSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	 TArray <  USoundBase *> ThunderSounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound)
	class USoundAttenuation* ThunderAttenuation;
private:
	/** The sound to play when it is raining.*/
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Sound, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	//class UAudioComponent* AudioComponent;
public:
	float Time;

	void PlayThunder(FVector pos);

public:
	void PostRegisterAllComponents() override;
	void PostInitProperties() override;
	void PostLoad() override;
	void PostInitializeComponents() override;
	bool ShouldTickIfViewportsOnly() const override 
	{
		return true;
	}
	void TickActor( float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction ) override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	 FBox GetComponentsBoundingBox(bool bNonColliding = false) const override;
protected:
	bool IsActiveActor();
	void TransferProperties();
	void UpdateVolumes();
	int latest_thunderbolt_id;
	bool bSimulVersion4_1;
	void FillMaterialParameters();
public:
	void SetDefaultTextures();
	bool initializeDefaults;
	float wetness;

public:
	/** Returns AudioComponent subobject **/
	//class UAudioComponent* GetRainAudioComponent() const;
	//class UAudioComponent* GetThunderAudioComponent() const;
	bool IsAnyActorActive() ;
};
