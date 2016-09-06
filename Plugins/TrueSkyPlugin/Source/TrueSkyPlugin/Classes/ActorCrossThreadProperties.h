#pragma once
#include "TrueSkyPluginPrivatePCH.h"

struct ActorCrossThreadProperties
{
	ActorCrossThreadProperties()
		:Destroyed(false)
		,Visible(false)
		,Reset(true)
		,Playing(false)
		,MetresPerUnit(0.01f)
		,Brightness(1.0f)
		,SimpleCloudShadowing(0.0f)
		,SimpleCloudShadowSharpness(0.01f)
		,CloudThresholdDistanceKm(0.1f)
		,DownscaleFactor(2)
		,Amortization(1)
		,AtmosphericsAmortization(4)
		,activeSequence(NULL)
		,MoonTexture(NULL)
		,CosmicBackgroundTexture(NULL)
		,InscatterRT(NULL)
		,LossRT(NULL)
		,CloudVisibilityRT(NULL)
		,CloudShadowRT(nullptr)
		,RainCubemap(NULL)
		,Time(0.0f)
		,DepthBlending(true)
		,MaxSunRadiance(150000.0f)
		,InterpolationMode(0)
		,InterpolationTimeSeconds(10.0f)
		,MinimumStarPixelSize(2.0f)
		,PrecipitationOptions(0)
		,ShareBuffersForVR(true)
		,MaximumResolution(0)
		,DepthSamplingPixelRange(1.0f)
	{
		memset(lightningBolts,0,sizeof(simul::LightningBolt)*4);
	}
	bool Destroyed;
	bool Visible;
	bool Reset;
	bool Playing;
	float MetresPerUnit;
	float Brightness;
	float SimpleCloudShadowing;
	float SimpleCloudShadowSharpness;
	float CloudThresholdDistanceKm;
	int DownscaleFactor;
	int Amortization;
	int AtmosphericsAmortization;
	class UTrueSkySequenceAsset *activeSequence;
	class UTexture2D* MoonTexture;
	class UTexture2D* CosmicBackgroundTexture;
	class UTextureRenderTarget2D* InscatterRT;
	class UTextureRenderTarget2D* LossRT;
	class UTextureRenderTarget2D* CloudVisibilityRT;
	class UTextureRenderTarget2D* CloudShadowRT;
	class UTexture* RainCubemap;
	float Time;
	FTransform Transform;
	simul::LightningBolt lightningBolts[4];
	bool DepthBlending;
	float MaxSunRadiance;
	int InterpolationMode;
	float InterpolationTimeSeconds;
	float MinimumStarPixelSize;
	uint8_t PrecipitationOptions;
	bool ShareBuffersForVR;
	int MaximumResolution;
	float DepthSamplingPixelRange;

};
extern ActorCrossThreadProperties *GetActorCrossThreadProperties();