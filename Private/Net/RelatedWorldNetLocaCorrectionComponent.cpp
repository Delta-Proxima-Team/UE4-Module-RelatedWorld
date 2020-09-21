// Copyright Delta-Proxima Team (c) 2007-2020

#include "Net/RelatedWorldNetLocCorrectionComponent.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsReplication.h"
#include "WorldDirector.h"
#include "RelatedWorld.h"

#include "GameFramework/Character.h"

void HOOK_AActor_OnRep_ReplicatedMovement(UObject* Context, FFrame& Stack, RESULT_DECL)
{
	AActor* Caller = Cast<AActor>(Context);

	if (Caller != nullptr)
	{
		P_FINISH;
		P_NATIVE_BEGIN;
		URelatedWorldNetLocCorrectionComponent* HookComp = Cast<URelatedWorldNetLocCorrectionComponent>(Caller->GetComponentByClass(URelatedWorldNetLocCorrectionComponent::StaticClass()));

		if (HookComp != nullptr)
		{
			HookComp->OnRep_ReplicatedMovement();
		}
		else
		{
			Caller->OnRep_ReplicatedMovement();
		}
		P_NATIVE_END;
	}
}

void HOOK_ACharacter_ClientAdjustPosition(UObject* Context, FFrame& Stack, RESULT_DECL)
{
	ACharacter* Caller = Cast<ACharacter>(Context);

	if (Caller != nullptr)
	{
		P_GET_PROPERTY(FFloatProperty, TimeStamp);
		P_GET_STRUCT(FVector, NewLoc);
		P_GET_STRUCT(FVector, NewVel);
		P_GET_OBJECT(UPrimitiveComponent, NewBase);
		P_GET_PROPERTY(FNameProperty, NewBaseBoneName);
		P_GET_PROPERTY(FBoolProperty, bHasBase);
		P_GET_PROPERTY(FBoolProperty, bBaseRelativePosition);
		P_GET_PROPERTY(FInt8Property, ServerMovementMode);
		P_FINISH;
		P_NATIVE_BEGIN;
		Caller->ClientAdjustPosition_Implementation(TimeStamp, NewLoc, NewVel, NewBase, NewBaseBoneName, bHasBase, bBaseRelativePosition, ServerMovementMode);
		P_NATIVE_END;
	}
}

URelatedWorldNetLocCorrectionComponent::URelatedWorldNetLocCorrectionComponent()
{
	bInitialReplication = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	bNeedCorrection = false;
}

void URelatedWorldNetLocCorrectionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(URelatedWorldNetLocCorrectionComponent, bInitialReplication, COND_InitialOnly);
	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, bNeedCorrection);
	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, ActorOwner);
	DOREPLIFETIME(URelatedWorldNetLocCorrectionComponent, RelatedWorldLocation);
}

void URelatedWorldNetLocCorrectionComponent::InitializeComponent()
{
	Super::InitializeComponent();

	if (IsValid(GetOwner()) && !GetOwner()->HasAnyFlags(RF_ClassDefaultObject))
	{
		ActorOwner = GetOwner();
		RelatedWorld = UWorldDirector::Get()->GetRelatedWorldFromActor(ActorOwner);

		if (RelatedWorld)
		{
			bInitialReplication = true;
			bNeedCorrection = true;
		}
		else
		{
			bNeedCorrection = false;
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (ActorOwner && !ActorOwner->IsPendingKill() && GetOwnerRole() == ROLE_Authority)
	{
		if (RelatedWorld)
		{
			FIntVector WorldLocation = RelatedWorld->GetWorldTranslation();
			
			if (WorldLocation != RelatedWorldLocation)
			{
				NotifyWorldLocationChanged(WorldLocation);
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::OnRep_Initial()
{
	if (ActorOwner && !ActorOwner->IsPendingKill() && bNeedCorrection)
	{
		USceneComponent* RootComponent = ActorOwner->GetRootComponent();

		if (RootComponent)
		{
			FVector Location = RootComponent->GetComponentLocation();
			UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());

			if ((PrimComp && PrimComp->IsSimulatingPhysics()) || Location == FVector::ZeroVector)
			{
				Location = URelatedWorldUtils::CONVERT_RelToWorld(RelatedWorldLocation, Location);
				RootComponent->SetWorldLocation(Location);
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::OnRep_RelatedWorldLocation()
{
	//Recalculate client position
	OnRep_ReplicatedMovement();
}

void URelatedWorldNetLocCorrectionComponent::OnRep_ReplicatedMovement()
{
	if (!ActorOwner || ActorOwner->IsPendingKill() || !ActorOwner->IsReplicatingMovement())
		return;

	if (!bNeedCorrection)
	{
		ActorOwner->OnRep_ReplicatedMovement();
	}

	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();
	USceneComponent* RootComponent = ActorOwner->GetRootComponent();

	if (RootComponent)
	{
		if (SavedbRepPhysics != LocalRepMovement.bRepPhysics)
		{
			// Turn on/off physics sim to match server.
			SyncReplicatedPhysicsSimulation();
			SavedbRepPhysics = LocalRepMovement.bRepPhysics;
		}

		if (LocalRepMovement.bRepPhysics)
		{
			// Sync physics state
			checkSlow(RootComponent->IsSimulatingPhysics());
			// If we are welded we just want the parent's update to move us.
			UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
			if (!RootPrimComp || !RootPrimComp->IsWelded())
			{
				PostNetReceivePhysicState();
			}
		}
		else
		{
			// Attachment trumps global position updates, see GatherCurrentMovement().
			if (!RootComponent->GetAttachParent())
			{
				if (GetOwnerRole() == ROLE_SimulatedProxy)
				{
#if ENABLE_NAN_DIAGNOSTIC
					if (LocalRepMovement.Location.ContainsNaN())
					{
						logOrEnsureNanError(TEXT("AActor::OnRep_ReplicatedMovement found NaN in ReplicatedMovement.Location"));
					}
					if (LocalRepMovement.Rotation.ContainsNaN())
					{
						logOrEnsureNanError(TEXT("AActor::OnRep_ReplicatedMovement found NaN in ReplicatedMovement.Rotation"));
					}
#endif

					ActorOwner->PostNetReceiveVelocity(LocalRepMovement.LinearVelocity);
					PostNetReceiveLocationAndRotation();
				}
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::SyncReplicatedPhysicsSimulation()
{
	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();
	USceneComponent* RootComponent = ActorOwner->GetRootComponent();

	if (ActorOwner->IsReplicatingMovement() && RootComponent && (RootComponent->IsSimulatingPhysics() != LocalRepMovement.bRepPhysics))
	{
		UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(RootComponent);
		if (RootPrimComp)
		{
			RootPrimComp->SetSimulatePhysics(LocalRepMovement.bRepPhysics);

			if (!LocalRepMovement.bRepPhysics)
			{
				if (UWorld* World = GetWorld())
				{
					if (FPhysScene* PhysScene = World->GetPhysicsScene())
					{
						if (FPhysicsReplication* PhysicsReplication = PhysScene->GetPhysicsReplication())
						{
							PhysicsReplication->RemoveReplicatedTarget(RootPrimComp);
						}
					}
				}
			}
		}
	}
}

void URelatedWorldNetLocCorrectionComponent::PostNetReceivePhysicState()
{
	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();
	UPrimitiveComponent* RootPrimComp = Cast<UPrimitiveComponent>(ActorOwner->GetRootComponent());
	if (RootPrimComp)
	{
		FRigidBodyState NewState;
		
		FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(LocalRepMovement.Location, ActorOwner);
		NewLocation = URelatedWorldUtils::CONVERT_RelToWorld(RelatedWorldLocation, NewLocation);

		NewState.Position = NewLocation;
		NewState.Quaternion = LocalRepMovement.Rotation.Quaternion();
		NewState.LinVel = LocalRepMovement.LinearVelocity;
		NewState.AngVel = LocalRepMovement.AngularVelocity;
		NewState.Flags = (LocalRepMovement.bSimulatedPhysicSleep ? ERigidBodyFlags::Sleeping : ERigidBodyFlags::None) | ERigidBodyFlags::NeedsUpdate;

		RootPrimComp->SetRigidBodyReplicatedTarget(NewState);
	}
}

void URelatedWorldNetLocCorrectionComponent::PostNetReceiveLocationAndRotation()
{
	const FRepMovement& LocalRepMovement = ActorOwner->GetReplicatedMovement();

	FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(LocalRepMovement.Location, ActorOwner);
	NewLocation = URelatedWorldUtils::CONVERT_RelToWorld(RelatedWorldLocation, NewLocation);

	USceneComponent* RootComponent = ActorOwner->GetRootComponent();

	if (RootComponent && RootComponent->IsRegistered() && (NewLocation != ActorOwner->GetActorLocation() || LocalRepMovement.Rotation != ActorOwner->GetActorRotation()))
	{
		ActorOwner->SetActorLocationAndRotation(NewLocation, LocalRepMovement.Rotation, /*bSweep=*/ false);
	}
}

void URelatedWorldNetLocCorrectionComponent::NotifyWorldChanged(URelatedWorld* NewWorld)
{
	RelatedWorld = NewWorld;
	NotifyWorldLocationChanged(NewWorld->GetWorldTranslation());
}

void URelatedWorldNetLocCorrectionComponent::NotifyWorldLocationChanged(const FIntVector& NewWorldLocation)
{
	RelatedWorldLocation = NewWorldLocation;
}
