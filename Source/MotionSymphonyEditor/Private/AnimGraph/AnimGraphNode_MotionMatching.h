// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraph/AnimNode_MotionMatching.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "AnimGraphNode_MotionMatching.generated.h"


UCLASS(BlueprintType, Blueprintable)
class MOTIONSYMPHONYEDITOR_API UAnimGraphNode_MotionMatching : public UAnimGraphNode_AssetPlayerBase
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_MotionMatching Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	//virtual void CreateOutputPins() override;
	//~ End UEdGraphNode Interface.

	//~ Begin UAnimGraphNode_Base Interface
	virtual FString GetNodeCategory() const override;
	virtual void ValidateAnimNodeDuringCompilation(class USkeleton* ForSkeleton, class FCompilerResultsLog& MessageLog) override;
	virtual void PreloadRequiredAssets() override;
	virtual void BakeDataDuringCompilation(class FCompilerResultsLog& MessageLog) override;
	virtual bool DoesSupportTimeForTransitionGetter() const override;
	virtual UAnimationAsset* GetAnimationAsset() const override;
	virtual const TCHAR* GetTimePropertyName() const override;
	virtual UScriptStruct* GetTimePropertyStruct() const override;
	virtual void GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap) override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual EAnimAssetHandlerType SupportsAssetClass(const UClass* AssetClass) const override;
	//~ End UAnimGraphNode_Base Interface

	// UK2Node interface
	virtual void GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const override;
	// End of UK2Node interface

	// UAnimGraphNode_AssetPlayerBase interface
	virtual void SetAnimationAsset(UAnimationAsset* Asset) override;
#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25 
	virtual void OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext,
		IAnimBlueprintGeneratedClassCompiledData& OutCompiledData) override;
#endif
	// End of UAnimGraphNode_AssetPlayerBase interface

private:
	static FText GetTitleGivenAssetInfo(const FText& AssetName, bool bKnownToBeAdditive);
	FText GetNodeTitleForMotionData(ENodeTitleType::Type TitleType, UMotionDataAsset* InMotionData) const;

	virtual FString GetControllerDescription() const;

	/** Constructing FText strings can be costly, so we cache the node's title */
	FNodeTitleTextTable CachedNodeTitles;
	
	/** Used for filtering in the Blueprint context menu when the sequence asset this node uses is unloaded */
	FString UnloadedSkeletonName;
};
