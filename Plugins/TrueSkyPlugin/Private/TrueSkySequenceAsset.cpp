#include "TrueSkyPluginPrivatePCH.h"
#include "TrueSkySequenceAsset.h"

UTrueSkySequenceAsset::UTrueSkySequenceAsset(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{

}

void UTrueSkySequenceAsset::BeginDestroy()
{
	Super::BeginDestroy();
}