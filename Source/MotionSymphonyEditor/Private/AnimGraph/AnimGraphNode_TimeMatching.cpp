// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "AnimGraphNode_TimeMatching.h"
#include "AnimationGraphSchema.h"
#include "EditorCategoryUtils.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraphSchema_K2_Actions.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"
#include "GraphEditorActions.h"
#include "ARFilter.h"
#include "AssetRegistryModule.h"
#include "BlueprintActionFilter.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "Animation/AnimComposite.h"

#define LOCTEXT_NAMESPACE "MotionSymphonyNodes"

UAnimGraphNode_TimeMatching::UAnimGraphNode_TimeMatching(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FLinearColor UAnimGraphNode_TimeMatching::GetNodeTitleColor() const
{
	return FColor(200, 100, 100);
}

FText UAnimGraphNode_TimeMatching::GetTooltipText() const
{
	if (!Node.Sequence)
	{
		return LOCTEXT("NodeToolTip", "Time Matching");
	}

	//Additive Not Supported
	return GetTitleGivenAssetInfo(FText::FromString(Node.Sequence->GetPathName()), false);
}

FText UAnimGraphNode_TimeMatching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (Node.Sequence == nullptr)
	{
		// we may have a valid variable connected or default pin value
		UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, Sequence));
		if (SequencePin && SequencePin->LinkedTo.Num() > 0)
		{
			return LOCTEXT("TimeMatchNodeTitleVariable", "Time Matching");
		}
		else if (SequencePin && SequencePin->DefaultObject != nullptr)
		{
			return GetNodeTitleForSequence(TitleType, CastChecked<UAnimSequenceBase>(SequencePin->DefaultObject));
		}
		else
		{
			return LOCTEXT("TimeMatchNullTitle", "Time Matching (None)");
		}
	}
	else
	{
		return GetNodeTitleForSequence(TitleType, Node.Sequence);
	}
}

EAnimAssetHandlerType UAnimGraphNode_TimeMatching::SupportsAssetClass(const UClass* AssetClass) const
{
	if (AssetClass->IsChildOf(UAnimSequence::StaticClass()) || AssetClass->IsChildOf(UAnimComposite::StaticClass()))
	{
		return EAnimAssetHandlerType::Supported;
	}
	else
	{
		return EAnimAssetHandlerType::NotSupported;
	}
}

FString UAnimGraphNode_TimeMatching::GetNodeCategory() const
{
	return FString("Motion Symphony (Experimental)");
}

void UAnimGraphNode_TimeMatching::GetNodeContextMenuActions(class UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->bIsDebugging)
	{
		// add an option to convert to single frame
		//FToolMenuSection& Section = Menu->AddSection("AnimGraphNodeTimeMatching", NSLOCTEXT("A3Nodes", "TimeMatchHeading", "Time Matching"));
		//Section.AddMenuEntry(FGraphEditorCommands::Get().OpenRelatedAsset);
	}

}

void UAnimGraphNode_TimeMatching::SetAnimationAsset(UAnimationAsset * Asset)
{
	if (UAnimSequenceBase* Seq = Cast<UAnimSequenceBase>(Asset))
	{
		Node.Sequence = Seq;
	}
}

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25 
void UAnimGraphNode_TimeMatching::OnProcessDuringCompilation(IAnimBlueprintCompilationContext& InCompilationContext, IAnimBlueprintGeneratedClassCompiledData& OutCompiledData)
{

}
#endif

FText UAnimGraphNode_TimeMatching::GetTitleGivenAssetInfo(const FText & AssetName, bool bKnownToBeAdditive)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), AssetName);

	return FText::Format(LOCTEXT("TimeMatchNodeTitle", "{AssetName} \n Time Matching"), Args);
}

FText UAnimGraphNode_TimeMatching::GetNodeTitleForSequence(ENodeTitleType::Type TitleType, UAnimSequenceBase * InSequence) const
{
	const FText BasicTitle = GetTitleGivenAssetInfo(FText::FromName(InSequence->GetFName()), false);

	if (SyncGroup.GroupName == NAME_None)
	{
		return BasicTitle;
	}
	else
	{
		const FText SyncGroupName = FText::FromName(SyncGroup.GroupName);

		FFormatNamedArguments Args;
		Args.Add(TEXT("Title"), BasicTitle);
		Args.Add(TEXT("SyncGroup"), SyncGroupName);

		if (TitleType == ENodeTitleType::FullTitle)
		{
			return FText::Format(LOCTEXT("TimeMatchNodeGroupWithSubtitleFull", "{Title}\nSync group {SyncGroup}"), Args);
		}
		else
		{
			return FText::Format(LOCTEXT("TimeMatchNodeGroupWithSubtitleList", "{Title} (Sync group {SyncGroup})"), Args);
		}
	}
}

FString UAnimGraphNode_TimeMatching::GetControllerDescription() const
{
	return TEXT("Time Matching Animation Node");
}

//void UAnimGraphNode_TimeMatching::CreateOutputPins()
//{
//	const UAnimationGraphSchema* Schema = GetDefault<UAnimationGraphSchema>();
//	CreatePin(EGPD_Output, Schema->PC_Struct, TEXT(""), FPoseLink::StaticStruct(), /*bIsArray=*/ false, /*bIsReference=*/ false, TEXT("Pose"));
//}



void UAnimGraphNode_TimeMatching::ValidateAnimNodeDuringCompilation(USkeleton * ForSkeleton, FCompilerResultsLog & MessageLog)
{
	Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);

	UAnimSequenceBase* SequenceToCheck = Node.Sequence;
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, Sequence));
	if (SequencePin != nullptr && SequenceToCheck == nullptr)
	{
		SequenceToCheck = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	if (SequenceToCheck == nullptr)
	{
		// we may have a connected node
		if (SequencePin == nullptr || SequencePin->LinkedTo.Num() == 0)
		{
			MessageLog.Error(TEXT("@@ references an unknown sequence"), this);
		}
	}
	else if (SupportsAssetClass(SequenceToCheck->GetClass()) == EAnimAssetHandlerType::NotSupported)
	{
		MessageLog.Error(*FText::Format(LOCTEXT("UnsupportedAssetError", "@@ is trying to play a {0} as a sequence, which is not allowed."), SequenceToCheck->GetClass()->GetDisplayNameText()).ToString(), this);
	}
	else if (SequenceToCheck->IsValidAdditive())
	{
		MessageLog.Error(TEXT("@@ is trying to play an additive animation sequence, which is not allowed."), this);
	}
	else
	{
		USkeleton* SeqSkeleton = SequenceToCheck->GetSkeleton();
		if (SeqSkeleton && // if anim sequence doesn't have skeleton, it might be due to anim sequence not loaded yet, @todo: wait with anim blueprint compilation until all assets are loaded?
			!SeqSkeleton->IsCompatible(ForSkeleton))
		{
			MessageLog.Error(TEXT("@@ references sequence that uses different skeleton @@"), this, SeqSkeleton);
		}
	}
}

void UAnimGraphNode_TimeMatching::PreloadRequiredAssets()
{
	PreloadObject(Node.Sequence);
	Super::PreloadRequiredAssets();
}

void UAnimGraphNode_TimeMatching::BakeDataDuringCompilation(FCompilerResultsLog & MessageLog)
{
	UAnimBlueprint* AnimBlueprint = GetAnimBlueprint();

#if ENGINE_MAJOR_VERSION > 4 || ENGINE_MINOR_VERSION > 25 
	Node.GroupName = SyncGroup.GroupName;
#else
	Node.GroupIndex = AnimBlueprint->FindOrAddGroup(SyncGroup.GroupName);
#endif

	Node.GroupRole = SyncGroup.GroupRole;
}

void UAnimGraphNode_TimeMatching::GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets) const
{
	if (Node.Sequence)
	{
		HandleAnimReferenceCollection(Node.Sequence, AnimationAssets);
	}
}

void UAnimGraphNode_TimeMatching::ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	HandleAnimReferenceReplacement(Node.Sequence, AnimAssetReplacementMap);
}

bool UAnimGraphNode_TimeMatching::DoesSupportTimeForTransitionGetter() const
{
	return true;
}

UAnimationAsset * UAnimGraphNode_TimeMatching::GetAnimationAsset() const
{
	UAnimSequenceBase* Sequence = Node.Sequence;
	UEdGraphPin* SequencePin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_SequencePlayer, Sequence));
	if (SequencePin != nullptr && Sequence == nullptr)
	{
		Sequence = Cast<UAnimSequenceBase>(SequencePin->DefaultObject);
	}

	return Sequence;
}

const TCHAR* UAnimGraphNode_TimeMatching::GetTimePropertyName() const
{
	return TEXT("InternalTimeAccumulator");
}

UScriptStruct* UAnimGraphNode_TimeMatching::GetTimePropertyStruct() const
{
	return FAnimNode_TimeMatching::StaticStruct();
}

void UAnimGraphNode_TimeMatching::CustomizePinData(UEdGraphPin* Pin, FName SourcePropertyName, int32 ArrayIndex) const
{
	Super::CustomizePinData(Pin, SourcePropertyName, ArrayIndex);

	if (Pin->PinName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TimeMatching, PlayRate))
	{
		if (!Pin->bHidden)
		{
			// Draw value for PlayRateBasis if the pin is not exposed
			UEdGraphPin* PlayRateBasisPin = FindPin(GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TimeMatching, PlayRateBasis));
			if (!PlayRateBasisPin || PlayRateBasisPin->bHidden)
			{
				if (Node.PlayRateBasis != 1.f)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
					Args.Add(TEXT("PlayRateBasis"), FText::AsNumber(Node.PlayRateBasis));
					Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_TimeMatching_PlayRateBasis_Value", "({PinFriendlyName} / {PlayRateBasis})"), Args);
				}
			}
			else // PlayRateBasisPin is visible
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("PinFriendlyName"), Pin->PinFriendlyName);
				Pin->PinFriendlyName = FText::Format(LOCTEXT("FAnimNode_TimeMatching_PlayRateBasis_Name", "({PinFriendlyName} / PlayRateBasis)"), Args);
			}

			Pin->PinFriendlyName = Node.PlayRateScaleBiasClamp.GetFriendlyName(Pin->PinFriendlyName);
		}
	}
}

void UAnimGraphNode_TimeMatching::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);

	// Reconstruct node to show updates to PinFriendlyNames.
	if ((PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FAnimNode_TimeMatching, PlayRateBasis))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bMapRange))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Min))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputRange, Max))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, Scale))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, Bias))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bClampResult))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, ClampMin))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, ClampMax))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, bInterpResult))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, InterpSpeedIncreasing))
		|| (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(FInputScaleBiasClamp, InterpSpeedDecreasing)))
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UAnimGraphNode_TimeMatching::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(GetClass());
	NodeSpawner->DefaultMenuSignature.MenuName = FText::FromString(TEXT("Time Matching"));
	NodeSpawner->DefaultMenuSignature.Tooltip = FText::FromString(TEXT("Sequence player which uses Time matching to pick it's starting point"));
	ActionRegistrar.AddBlueprintAction(NodeSpawner);
}


#undef LOCTEXT_NAMESPACE
