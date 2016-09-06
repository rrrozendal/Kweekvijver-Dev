#include "TrueSkyPluginPrivatePCH.h"
#include "TrueSkyComponent.h"

UTrueSkyComponent::UTrueSkyComponent(const class FObjectInitializer& PCIP):Super(PCIP)
{
}

void UTrueSkyComponent::OnRegister()
{
    Super::OnRegister();
}

void UTrueSkyComponent::InitializeComponent()
{
    Super::InitializeComponent();
}

void UTrueSkyComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
    Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
}

void UTrueSkyComponent::OnUnregister()
{
    Super::OnUnregister();
}

