#include "TrueSkyEditorPluginPrivatePCH.h"

#include "TrueSkySequenceFactory.h"
#include "TrueSkySequenceAsset.h"

UTrueSkySequenceFactory::UTrueSkySequenceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UTrueSkySequenceAsset::StaticClass();
}

UObject* UTrueSkySequenceFactory::FactoryCreateNew(UClass* Class,UObject* InParent,FName Name,EObjectFlags Flags,UObject* Context,FFeedbackContext* Warn)
{
	UTrueSkySequenceAsset* NewSequence = NewObject<UTrueSkySequenceAsset>(InParent,Class,Name,Flags);
	return NewSequence;
}

FName UTrueSkySequenceFactory::GetNewAssetThumbnailOverride() const
{
	return TEXT("ClassThumbnail.TrueSkySequenceAsset");
}