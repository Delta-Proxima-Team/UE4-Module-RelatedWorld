// Copyright Delta-Proxima Team (c) 2007-2020

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Modules/ModuleManager.h"

#include "RelatedWorldModuleInterface.h"
#include "WorldDirector.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldDirector, Log, All);

UCLASS(BlueprintType)
class RELATEDWORLD_API URelatedWorld : public UObject, public FTickableGameObject
{
	GENERATED_BODY()

public:
	void SetNetworked(bool bNetworked) { bIsNetworkedWorld = bNetworked; }
	void SetPersistentWorld(UWorld* World) { PersistentWorld = World; }

	void HandleBeginPlay();

	void SetContext(FWorldContext* Context) { _Context = Context; }
	FWorldContext* Context() const { return _Context; }

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		AActor* SpawnActor(UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		bool MoveActorToWorld(AActor* InActor, bool bTranslateLocation);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void SetWorldOrigin(FIntVector NewOrigin);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void TranslateWorld(FIntVector NewLocation);

	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		bool IsNetworkedWorld() const { return bIsNetworkedWorld; }

	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		FIntVector GetWorldTranslation() const { return WorldLocation; }

	void Tick(float DeltaSeconds) override;
	bool IsTickable() const override;
	bool IsTickableInEditor() const override { return false; };
	bool IsTickableWhenPaused() const override { return true; };
	TStatId GetStatId() const override { return TStatId(); };
	UWorld* GetWorld() const override;

private:
	FWorldContext* _Context;
	UWorld* PersistentWorld;
	bool bIsNetworkedWorld;
	FIntVector WorldLocation;

};

UCLASS(BlueprintType)
class RELATEDWORLD_API UWorldDirector : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		static UWorldDirector* GetWorldDirector()
		{
			return FModuleManager::LoadModuleChecked<IRelatedWorldModule>("RelatedWorld").GetWorldDirector();
		}

	UFUNCTION(BlueprintCallable, Category = "WorldDirector", Meta=(WorldContext="WorldContextObject"))
		URelatedWorld* LoadRelatedLevel(UObject* WorldContextObject, FName LevelName, FIntVector LevelLocation, bool IsNetWorld = true);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector", Meta = (WorldContext = "WorldContextObject"))
		URelatedWorld* CreateAbstractWorld(UObject* WorldContextObject, FName WorldName, FIntVector LevelLocation, bool IsNetWorld = true);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadAllRelatedLevels();

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadRelatedLevelByName(FName LevelName);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		void UnloadRelatedLevel(URelatedWorld* RelatedWorld);

	UFUNCTION(BlueprintCallable, Category = "WorldDirector")
		AActor* SpawnActor(URelatedWorld* TargetWorld, UClass* Class, const FTransform& SpawnTransform, ESpawnActorCollisionHandlingMethod CollisionHandlingOverride, AActor* Owner);

	UFUNCTION(BlueprintPure, Category = "WorldDirector")
		URelatedWorld* GetRelatedWorldFromActor(AActor* InActor) const;

private:
	TMap<FName, URelatedWorld*> Worlds;

};
