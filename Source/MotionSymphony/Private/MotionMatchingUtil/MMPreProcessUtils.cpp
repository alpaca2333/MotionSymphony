// Copyright 2020-2021 Kenneth Claassen. All Rights Reserved.

#include "MMPreProcessUtils.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Enumerations/EMotionMatchingEnums.h"

#if WITH_EDITOR
#include "AnimationBlueprintLibrary.h"
#endif

void FMMPreProcessUtils::ExtractRootMotionParams(FRootMotionMovementParams& OutRootMotion, 
	const TArray<FBlendSampleData>& BlendSampleData, const float BaseTime, const float DeltaTime, const bool AllowLooping)
{
	for (const FBlendSampleData& Sample : BlendSampleData)
	{
		float SampleWeight = Sample.GetWeight();

		if (SampleWeight > 0.0001f)
		{
			FTransform RootDeltaTransform = Sample.Animation->ExtractRootMotion(BaseTime, DeltaTime, AllowLooping);
			RootDeltaTransform.NormalizeRotation();
			OutRootMotion.AccumulateWithBlend(RootDeltaTransform, SampleWeight);
		}
	}
}

void FMMPreProcessUtils::ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity,
	UAnimSequence* AnimSequence, const float Time, const float PoseInterval)
{
	if (!AnimSequence)
	{
		OutRootVelocity = FVector::ZeroVector;
		OutRootRotVelocity = 0.0f;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform RootDeltaTransform = AnimSequence->ExtractRootMotion(StartTime, PoseInterval, false);
	RootDeltaTransform.NormalizeRotation();
	const FVector RootDeltaPos = RootDeltaTransform.GetTranslation();
	
	OutRootRotVelocity = RootDeltaTransform.GetRotation().Euler().Z / PoseInterval;
	OutRootVelocity = RootDeltaPos.GetSafeNormal() * (RootDeltaPos.Size() / PoseInterval);
}

void FMMPreProcessUtils::ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity, 
	const TArray<FBlendSampleData>& BlendSampleData, const float Time, const float PoseInterval)
{
	if (BlendSampleData.Num() == 0)
	{
		OutRootVelocity = FVector::ZeroVector;
		OutRootRotVelocity = 0.0f;
		return;
	}

	float StartTime = Time - (PoseInterval / 2.0f);

	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();

	ExtractRootMotionParams(RootMotionParams, BlendSampleData, StartTime, PoseInterval, false);

	FTransform RootDeltaTransform = RootMotionParams.GetRootMotionTransform();
	RootDeltaTransform.NormalizeRotation();
	FVector RootDeltaPos = RootDeltaTransform.GetTranslation();

	OutRootRotVelocity = RootDeltaTransform.GetRotation().Euler().Z / PoseInterval;
	OutRootVelocity = RootDeltaPos.GetSafeNormal() * (RootDeltaPos.Size() / PoseInterval);
}

void FMMPreProcessUtils::ExtractRootVelocity(FVector& OutRootVelocity, float& OutRootRotVelocity, 
	UAnimComposite* AnimComposite, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutRootVelocity = FVector::ZeroVector;
		OutRootRotVelocity = 0.0f;
		return;
	}

	float StartTime = Time - (PoseInterval / 2.0f);

	FRootMotionMovementParams RootMotionParams;
	AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, StartTime, StartTime + PoseInterval, RootMotionParams);

	FTransform RootDeltaTransform = RootMotionParams.GetRootMotionTransform();
	RootDeltaTransform.NormalizeRotation();
	FVector RootDeltaPos = RootDeltaTransform.GetTranslation();

	OutRootRotVelocity = RootDeltaTransform.GetRotation().Euler().Z / PoseInterval;
	OutRootVelocity = RootDeltaPos.GetSafeNormal() * (RootDeltaPos.Size() / PoseInterval);
}

void FMMPreProcessUtils::ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimSequence* AnimSequence, const float BaseTime, const float PointTime, 
	ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion)
{
	if (!AnimSequence)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	if ((int32)PastMethod > (int32)ETrajectoryPreProcessMethod::IgnoreEdges 
		&& PointAnimTime < 0.0f)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, -BaseTime, false);
		
		switch (PastMethod)
		{
			//Extrapolate the motion at the beginning of the clip
			case ETrajectoryPreProcessMethod::Extrapolate:
			{
				FTransform initialMotion = AnimSequence->ExtractRootMotion(0.05f, -0.05f, false);

				//transform the root delta by initial motion for a required number of Iterations
				int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime) / 0.05f);
				for (int32 i = 0; i < Iterations; ++i)
				{
					RootDelta *= initialMotion;
				}

			} break;

			case ETrajectoryPreProcessMethod::Animation:
			{
				if (PrecedingMotion == nullptr)
					break;

				FTransform precedingRootDelta = PrecedingMotion->ExtractRootMotion(PrecedingMotion->GetPlayLength(), PointAnimTime, false);

				RootDelta *= precedingRootDelta;

			} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, -BaseTime, 0.0f);

		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, DeltaTime, false);
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion)
{
	if (BlendSampleData.Num() == 0)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();

	FTransform RootDelta;

	if ((int32)PastMethod > (int32)ETrajectoryPreProcessMethod::IgnoreEdges
		&& PointAnimTime < 0.0f)
	{
		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, -BaseTime, false);

		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (PastMethod)
		{
			//Extrapolate the motion at the beginning of the clip
			case ETrajectoryPreProcessMethod::Extrapolate:
			{
				FRootMotionMovementParams ExtrapRootMotionParams;
				ExtrapRootMotionParams.Clear();

				ExtractRootMotionParams(ExtrapRootMotionParams, BlendSampleData, 0.05f, - 0.05f, false);
				FTransform InitialMotion = ExtrapRootMotionParams.GetRootMotionTransform();
				InitialMotion.NormalizeRotation();

				//transform the root delta by initial motion for a required number of Iterations
				int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime) / 0.05f);
				for (int32 i = 0; i < Iterations; ++i)
				{
					RootDelta *= InitialMotion;
				}

			} break;

			case ETrajectoryPreProcessMethod::Animation:
			{
				if (PrecedingMotion == nullptr)
					break;


				FTransform precedingRootDelta = PrecedingMotion->ExtractRootMotion(PrecedingMotion->GetPlayLength(), PointAnimTime, false);

				RootDelta *= precedingRootDelta;

			} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, -BaseTime, 0.0f);

		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, DeltaTime, false);
		RootDelta = RootMotionParams.GetRootMotionTransform();
		RootDelta.NormalizeRotation();
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractPastTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimComposite* AnimComposite, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod PastMethod, UAnimSequence* PrecedingMotion)
{
	if (!AnimComposite)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	if ((int32)PastMethod > (int32)ETrajectoryPreProcessMethod::IgnoreEdges
		&& PointAnimTime < 0.0f)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, 0.0f, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (PastMethod)
		{
			//Extrapolate the motion at the beginning of the clip
		case ETrajectoryPreProcessMethod::Extrapolate:
		{
			RootMotionParams.Clear();
			AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, 0.05f, 0.0f, RootMotionParams);
			FTransform InitialMotion = RootMotionParams.GetRootMotionTransform();

			//Transform the root delta by initial motion for a required number of Iterations
			int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime) / 0.05f);
			for (int32 i = 0; i < Iterations; ++i)
			{
				RootDelta *= InitialMotion;
			}

		} break;

		case ETrajectoryPreProcessMethod::Animation:
		{
			if (PrecedingMotion == nullptr)
				break;

			FTransform precedingRootDelta = PrecedingMotion->ExtractRootMotion(PrecedingMotion->GetPlayLength(), PointAnimTime, false);

			RootDelta *= precedingRootDelta;

		} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, -BaseTime, 0.0f);

		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, BaseTime + DeltaTime, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimSequence* AnimSequence, const float BaseTime, const float PointTime, 
	ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion)
{
	if (!AnimSequence)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	if ((int32)FutureMethod > (int32)ETrajectoryPreProcessMethod::IgnoreEdges
		&& PointAnimTime > AnimSequence->GetPlayLength())
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges

		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, AnimSequence->GetPlayLength() - BaseTime, false);

		switch (FutureMethod)
		{
			//Extrapolate the motion at the end of the clip
			case ETrajectoryPreProcessMethod::Extrapolate:
			{
				FTransform EndMotion = AnimSequence->ExtractRootMotion(AnimSequence->GetPlayLength() - 0.05f, 0.05f, false);

				//transform the root delta by initial motion for a required number of Iterations
				int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime - AnimSequence->GetPlayLength()) / 0.05f);
				for (int32 i = 0; i < Iterations; ++i)
				{
					RootDelta *= EndMotion;
				}

			} break;

			case ETrajectoryPreProcessMethod::Animation:
			{
				if (FollowingMotion == nullptr)
				{
					break;
				}

				FTransform FollowingRootDelta = FollowingMotion->ExtractRootMotion(0.0f, PointAnimTime - AnimSequence->GetPlayLength(), false);

				RootDelta *= FollowingRootDelta;

			} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float Time = FMath::Clamp(PointTime, 0.0f, AnimSequence->GetPlayLength() - BaseTime);

		RootDelta = AnimSequence->ExtractRootMotion(BaseTime, Time, false);
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion)
{
	if(BlendSampleData.Num() == 0)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;
	float AnimLength = BlendSampleData[0].Animation->GetPlayLength();

	//Root delta to the beginning of the clip
	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();
	FTransform RootDelta;

	

	if ((int32)FutureMethod > (int32)ETrajectoryPreProcessMethod::IgnoreEdges
		&& PointAnimTime > AnimLength)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, AnimLength - BaseTime, false);

		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (FutureMethod)
		{
			//Extrapolate the motion at the end of the clip
		case ETrajectoryPreProcessMethod::Extrapolate:
		{
			FRootMotionMovementParams ExtrapRootMotionParams;
			ExtrapRootMotionParams.Clear();
			ExtractRootMotionParams(ExtrapRootMotionParams, BlendSampleData, AnimLength - 0.05f, 0.05f, false);

			FTransform EndMotion = ExtrapRootMotionParams.GetRootMotionTransform();
			EndMotion.NormalizeRotation();

			//transform the root delta by initial motion for a required number of Iterations
			int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime - AnimLength) / 0.05f);
			for (int32 i = 0; i < Iterations; ++i)
			{
				RootDelta *= EndMotion;
			}

		} break;

		case ETrajectoryPreProcessMethod::Animation:
		{
			if (FollowingMotion == nullptr)
				break;

			FTransform FollowingRootDelta = FollowingMotion->ExtractRootMotion(0.0f, PointAnimTime - AnimLength, false);

			RootDelta *= FollowingRootDelta;

		} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float DeltaTime = FMath::Clamp(PointTime, 0.0f, AnimLength - BaseTime);
		
		ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, DeltaTime, false);
		RootDelta = RootMotionParams.GetRootMotionTransform();
		
	}

	RootDelta.NormalizeRotation();

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractFutureTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, UAnimComposite* AnimComposite, 
	const float BaseTime, const float PointTime, ETrajectoryPreProcessMethod FutureMethod, UAnimSequence* FollowingMotion)
{
	if (!AnimComposite)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	//Root delta to the beginning of the clip
	FTransform RootDelta;

	float SequenceLength = AnimComposite->GetPlayLength();
	if ((int32)FutureMethod > (int32)ETrajectoryPreProcessMethod::IgnoreEdges
		&& PointAnimTime > SequenceLength)
	{
		//Trajectory point Time is outside the bounds of the clip and we are not ignoring edges
		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, SequenceLength, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();

		switch (FutureMethod)
		{
			//Extrapolate the motion at the end of the clip
		case ETrajectoryPreProcessMethod::Extrapolate:
		{
			RootMotionParams.Clear();
			AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, SequenceLength - 0.05f,
				SequenceLength, RootMotionParams);

			FTransform EndMotion = RootMotionParams.GetRootMotionTransform();

			//transform the root delta by initial motion for a required number of Iterations
			int32 Iterations = FMath::RoundToInt(FMath::Abs(PointAnimTime - SequenceLength) / 0.05f);
			for (int32 i = 0; i < Iterations; ++i)
			{
				RootDelta *= EndMotion;
			}

		} break;

		case ETrajectoryPreProcessMethod::Animation:
		{
			if (FollowingMotion == nullptr)
				break;

			FTransform FollowingRootDelta = FollowingMotion->ExtractRootMotion(0.0f, PointAnimTime - SequenceLength, false);

			RootDelta *= FollowingRootDelta;

		} break;
		}
	}
	else
	{
		//Here the trajectory point either falls within the clip or we are ignoring edges
		//therefore, no fanciness is required
		float Time = FMath::Clamp(PointTime, 0.0f, SequenceLength - BaseTime);

		FRootMotionMovementParams RootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, BaseTime + Time, RootMotionParams);
		RootDelta = RootMotionParams.GetRootMotionTransform();
	}

	//Apply the calculated root deltas
	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimSequence* AnimSequence, const float BaseTime, const float PointTime)
{
	if (!AnimSequence)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	FTransform RootDelta = AnimSequence->ExtractRootMotion(BaseTime, PointTime, true);
	RootDelta.NormalizeRotation();

	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, const TArray<FBlendSampleData>& BlendSampleData, 
	const float BaseTime, const float PointTime)
{
	if (BlendSampleData.Num() == 0)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	FRootMotionMovementParams RootMotionParams;
	RootMotionParams.Clear();

	ExtractRootMotionParams(RootMotionParams, BlendSampleData, BaseTime, PointTime, true);

	FTransform RootDelta = RootMotionParams.GetRootMotionTransform();
	RootDelta.NormalizeRotation();

	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

void FMMPreProcessUtils::ExtractLoopingTrajectoryPoint(FTrajectoryPoint& OutTrajPoint, 
	UAnimComposite* AnimComposite, const float BaseTime, const float PointTime)
{
	if (!AnimComposite)
	{
		OutTrajPoint = FTrajectoryPoint();
		return;
	}

	float PointAnimTime = BaseTime + PointTime;

	FRootMotionMovementParams RootMotionParams;
	AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, BaseTime, PointAnimTime, RootMotionParams);

	float SequenceLength = AnimComposite->GetPlayLength();
	if (PointAnimTime < 0) //Extract from the previous loop and accumulate
	{
		FRootMotionMovementParams PastRootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, SequenceLength, 
			SequenceLength + PointAnimTime, PastRootMotionParams);

		RootMotionParams.Accumulate(PastRootMotionParams);
	}
	else if (PointAnimTime > SequenceLength) //Extract from the next loop and accumulate
	{
		FRootMotionMovementParams FutureRootMotionParams;
		AnimComposite->ExtractRootMotionFromTrack(AnimComposite->AnimationTrack, 0.0f, 
			PointAnimTime - SequenceLength, FutureRootMotionParams);

		RootMotionParams.Accumulate(FutureRootMotionParams);
	}

	FTransform RootDelta = RootMotionParams.GetRootMotionTransform();
	
	RootDelta.NormalizeRotation();

	OutTrajPoint = FTrajectoryPoint();
	OutTrajPoint.Position = RootDelta.GetTranslation();
	OutTrajPoint.RotationZ = RootDelta.GetRotation().Euler().Z;
}

#if WITH_EDITOR
void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, 
	UAnimSequence* AnimSequence, const int32 JointId, const float Time, const float PoseInterval)
{
	if (!AnimSequence)
	{
		OutJointData = FJointData();
		return;
	}

	FTransform JointTransform = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform, AnimSequence, JointId, Time);

	FVector JointVelocity = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity, AnimSequence, JointId, Time, PoseInterval);

	OutJointData = FJointData(JointTransform.GetLocation(), JointVelocity);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, const TArray<FBlendSampleData>& BlendSampleData,
	const int32 JointId, const float Time, const float PoseInterval)
{
	if(BlendSampleData.Num() == 0)
	{
		OutJointData = FJointData();
		return;
	}

	FTransform JointTransform = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform, BlendSampleData, JointId, Time);

	FVector JointVelocity = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity, BlendSampleData, JointId, Time, PoseInterval);

	OutJointData = FJointData(JointTransform.GetLocation(), JointVelocity);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, UAnimComposite* AnimComposite,
	const int32 JointId, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutJointData = FJointData();
		return;
	}

	FTransform JointTransform = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform, AnimComposite, JointId, Time);

	FVector JointVelocity = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity, AnimComposite, JointId, Time, PoseInterval);

	OutJointData = FJointData(JointTransform.GetLocation(), JointVelocity);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, UAnimSequence* AnimSequence,
	 const FBoneReference& BoneReference, const float Time, const float PoseInterval)
{
	if(!AnimSequence)
	{
		OutJointData = FJointData();
		return;
	}

	TArray<FName> BonesToRoot;
	UAnimationBlueprintLibrary::FindBonePathToRoot(AnimSequence, BoneReference.BoneName, BonesToRoot);
	BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root
	
	FTransform JointTransform_CS = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform_CS, AnimSequence, BonesToRoot, Time);
	
	FVector JointVelocity_CS = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity_CS, AnimSequence, BonesToRoot, Time, PoseInterval);

	OutJointData = FJointData(JointTransform_CS.GetLocation(), JointVelocity_CS);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, const TArray<FBlendSampleData>& BlendSampleData, 
	const FBoneReference& BoneReference, const float Time, const float PoseInterval)
{
	if (BlendSampleData.Num() == 0)
	{
		OutJointData = FJointData();
		return;
	}

	TArray<FName> BonesToRoot;
	UAnimationBlueprintLibrary::FindBonePathToRoot(BlendSampleData[0].Animation, BoneReference.BoneName, BonesToRoot);
	BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root

	FTransform JointTransform_CS = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform_CS, BlendSampleData, BonesToRoot, Time);
	//JointTransform_CS.SetLocation(JointTransform_CS.GetLocation() - RootBoneTransform.GetLocation());

	FVector JointVelocity_CS = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity_CS, BlendSampleData, BonesToRoot, Time, PoseInterval);

	OutJointData = FJointData(JointTransform_CS.GetLocation(), JointVelocity_CS);
}

void FMMPreProcessUtils::ExtractJointData(FJointData& OutJointData, UAnimComposite* AnimComposite, 
	const FBoneReference& BoneReference, const float Time, const float PoseInterval)
{
	if (!AnimComposite || AnimComposite->AnimationTrack.AnimSegments.Num() == 0)
	{
		OutJointData = FJointData();
		return;
	}

	UAnimSequence* CompositeFirstSequence = Cast<UAnimSequence>(AnimComposite->AnimationTrack.AnimSegments[0].AnimReference);

	if (!CompositeFirstSequence)
	{
		OutJointData = FJointData();
		return;
	}

	TArray<FName> BonesToRoot;
	UAnimationBlueprintLibrary::FindBonePathToRoot(CompositeFirstSequence, BoneReference.BoneName, BonesToRoot);
	BonesToRoot.RemoveAt(BonesToRoot.Num() - 1); //Removes the root

	FTransform JointTransform_CS = FTransform::Identity;
	GetJointTransform_RootRelative(JointTransform_CS, AnimComposite, BonesToRoot, Time);

	FVector JointVelocity_CS = FVector::ZeroVector;
	GetJointVelocity_RootRelative(JointVelocity_CS, AnimComposite, BonesToRoot, Time, PoseInterval);

	OutJointData = FJointData(JointTransform_CS.GetLocation(), JointVelocity_CS);
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector & OutJointVelocity, 
	UAnimSequence * AnimSequence, const int32 JointId, const float Time, const float PoseInterval)
{
	if(!AnimSequence)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimSequence, JointId, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimSequence, JointId, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, 
	const TArray<FBlendSampleData>& BlendSampleData, const int32 JointId, const float Time, const float PoseInterval)
{
	if(BlendSampleData.Num() == 0)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, BlendSampleData, JointId, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, BlendSampleData, JointId, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimComposite* AnimComposite,
	const int32 JointId, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimComposite, JointId, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimComposite, JointId, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimSequence* AnimSequence, 
	const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval)
{
	if(!AnimSequence)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimSequence, BonesToRoot, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimSequence, BonesToRoot, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, const TArray<FBlendSampleData>& BlendSampleData, 
	const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval)
{
	if (BlendSampleData.Num() == 0)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, BlendSampleData, BonesToRoot, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, BlendSampleData, BonesToRoot, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

void FMMPreProcessUtils::GetJointVelocity_RootRelative(FVector& OutJointVelocity, UAnimComposite* AnimComposite,
	const TArray<FName>& BonesToRoot, const float Time, const float PoseInterval)
{
	if (!AnimComposite)
	{
		OutJointVelocity = FVector::ZeroVector;
		return;
	}

	const float StartTime = Time - (PoseInterval / 2.0f);

	FTransform BeforeTransform = FTransform::Identity;
	GetJointTransform_RootRelative(BeforeTransform, AnimComposite, BonesToRoot, StartTime);

	FTransform AfterTransform = FTransform::Identity;
	GetJointTransform_RootRelative(AfterTransform, AnimComposite, BonesToRoot, StartTime + PoseInterval);

	OutJointVelocity = (AfterTransform.GetLocation() - BeforeTransform.GetLocation()) / PoseInterval;
}

int32 FMMPreProcessUtils::ConvertRefSkelBoneIdToAnimBoneId(const int32 BoneId, 
	const FReferenceSkeleton& FromRefSkeleton, const UAnimSequence* ToAnimSequence)
{
	if (!ToAnimSequence || BoneId == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const FName BoneName = FromRefSkeleton.GetBoneName(BoneId);

#if ENGINE_MAJOR_VERSION > 4
	const TArray<FBoneAnimationTrack>& AnimationTracks = ToAnimSequence->GetResampledTrackData();

	//for (const FBoneAnimationTrack& AnimTrack : AnimationTracks)
	for (int32 i = 0; i < AnimationTracks.Num(); ++i)
	{
		const FBoneAnimationTrack& AnimTrack = AnimationTracks[i];

		if (AnimTrack.Name == BoneName)
		{
			return i;
		}
	}

	return INDEX_NONE;
	//return ToAnimSequence->GetAnimationTrackNames().IndexOfByKey(BoneName);
#else
	return ToAnimSequence->GetAnimationTrackNames().IndexOfByKey(BoneName);
#endif
}

int32 FMMPreProcessUtils::ConvertBoneNameToAnimBoneId(const FName BoneName, const UAnimSequence* ToAnimSequence)
{
#if ENGINE_MAJOR_VERSION > 4
	const TArray<FBoneAnimationTrack>& AnimationTracks = ToAnimSequence->GetResampledTrackData();

	//for (const FBoneAnimationTrack& AnimTrack : AnimationTracks)
	for(int32 i = 0; i < AnimationTracks.Num(); ++i)
	{
		const FBoneAnimationTrack& AnimTrack = AnimationTracks[i];

		if (AnimTrack.Name == BoneName)
		{
			return i;
		}
	}

	return INDEX_NONE;
	//return ToAnimSequence->GetAnimationTrackNames().IndexOfByKey(BoneName);
#else
	return ToAnimSequence->GetAnimationTrackNames().IndexOfByKey(BoneName);
#endif
}


#endif


#if WITH_EDITOR
void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform & OutJointTransform,
	UAnimSequence * AnimSequence, const int32 JointId, const float Time)
{
	OutJointTransform = FTransform::Identity;

	if (!AnimSequence || JointId == INDEX_NONE || JointId == 0)
	{
		return;
	}
	
	FReferenceSkeleton RefSkeleton = AnimSequence->GetSkeleton()->GetReferenceSkeleton();

	if (RefSkeleton.IsValidIndex(JointId))
	{
		int32 ConvertedJointId = ConvertRefSkelBoneIdToAnimBoneId(JointId, RefSkeleton, AnimSequence);

		if (ConvertedJointId == INDEX_NONE)
		{
			return;
		}

		AnimSequence->GetBoneTransform(OutJointTransform, ConvertedJointId, Time, true);
		int32 CurrentJointId = JointId;

		while (RefSkeleton.GetRawParentIndex(CurrentJointId) != 0)
		{
			//Need to get parents by name
			const int32 ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
			ConvertedJointId = ConvertRefSkelBoneIdToAnimBoneId(ParentJointId, RefSkeleton, AnimSequence);
			
			FTransform ParentTransform;
			AnimSequence->GetBoneTransform(ParentTransform, ConvertedJointId, Time, true);

			OutJointTransform = OutJointTransform * ParentTransform;
			CurrentJointId = ParentJointId;
		}
	}
	else
	{
		OutJointTransform = FTransform::Identity;
	}
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutJointTransform, 
	const TArray<FBlendSampleData>& BlendSampleData, const int32 JointId, const float Time)
{
	OutJointTransform = FTransform::Identity;
	
	if (BlendSampleData.Num() == 0 || JointId == INDEX_NONE)
	{
		return;
	}

	for(const FBlendSampleData& Sample : BlendSampleData)
	{
		if(!Sample.Animation)
		{
			continue;
		}

		const FReferenceSkeleton& RefSkeleton = Sample.Animation->GetSkeleton()->GetReferenceSkeleton();

		if (!RefSkeleton.IsValidIndex(JointId))
		{
			continue;
		}

		const ScalarRegister VSampleWeight(Sample.GetWeight());
		
		int32 ConvertedJointId = ConvertRefSkelBoneIdToAnimBoneId(JointId, RefSkeleton, Sample.Animation);
		if (ConvertedJointId == INDEX_NONE)
		{
			continue;
		}

		FTransform AnimJointTransform;
		Sample.Animation->GetBoneTransform(AnimJointTransform, ConvertedJointId, Time, true);

		int32 CurrentJointId = JointId;

		if (CurrentJointId == 0)
		{
			OutJointTransform.Accumulate(AnimJointTransform, VSampleWeight);
			continue;
		}

		int32 ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
		while (ParentJointId != 0)
		{
			ConvertedJointId = ConvertRefSkelBoneIdToAnimBoneId(ParentJointId, RefSkeleton, Sample.Animation);

			FTransform ParentTransform;
			Sample.Animation->GetBoneTransform(ParentTransform, ConvertedJointId, Time, true);

			AnimJointTransform = AnimJointTransform * ParentTransform;
			CurrentJointId = ParentJointId;
			ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
		}

		OutJointTransform.Accumulate(AnimJointTransform, VSampleWeight);
	}

	OutJointTransform.NormalizeRotation();
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutJointTransform, 
	UAnimComposite* AnimComposite, const int32 JointId, const float Time)
{
	OutJointTransform = FTransform::Identity;

	if (!AnimComposite || JointId == INDEX_NONE || JointId == 0)
	{
		return;
	}

	float CumDuration = 0.0f;
	UAnimSequence* Sequence = nullptr;
	float NewTime = Time;
	for (int32 i = 0; i < AnimComposite->AnimationTrack.AnimSegments.Num(); ++i)
	{
		const FAnimSegment& AnimSegment = AnimComposite->AnimationTrack.AnimSegments[i];
		float Length = AnimSegment.AnimReference->GetPlayLength();

		if (Length + CumDuration > Time)
		{
			Sequence = Cast<UAnimSequence>(AnimSegment.AnimReference);
			break;
		}
		else
		{
			CumDuration += Length;
			NewTime -= Length;
		}
	}

	if (!Sequence)
	{
		return;
	}

	FReferenceSkeleton RefSkeleton = Sequence->GetSkeleton()->GetReferenceSkeleton();

	if (RefSkeleton.IsValidIndex(JointId))
	{
		int32 ConvertedJointId = ConvertRefSkelBoneIdToAnimBoneId(JointId, RefSkeleton, Sequence);
		if (ConvertedJointId == INDEX_NONE)
		{
			return;
		}
		Sequence->GetBoneTransform(OutJointTransform, ConvertedJointId, Time, true);
		int32 CurrentJointId = JointId;

		while (RefSkeleton.GetRawParentIndex(CurrentJointId) != 0)
		{
			//Need to get parents by name
			int32 ParentJointId = RefSkeleton.GetRawParentIndex(CurrentJointId);
			ConvertedJointId = ConvertRefSkelBoneIdToAnimBoneId(ParentJointId, RefSkeleton, Sequence);

			FTransform ParentTransform;
		
			Sequence->GetBoneTransform(ParentTransform, ConvertedJointId, NewTime, true);

			OutJointTransform = OutJointTransform * ParentTransform;
			CurrentJointId = ParentJointId;
		}
	}
	else
	{
		OutJointTransform = FTransform::Identity;
	}
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutTransform,
	UAnimSequence* AnimSequence, const TArray<FName>& BonesToRoot, const float Time)
{
	OutTransform = FTransform::Identity;

	if (!AnimSequence)
	{
		return;
	}

	for (const FName& BoneName : BonesToRoot)
	{
		FTransform BoneTransform;
		const int32 ConvertedBoneIndex = ConvertBoneNameToAnimBoneId(BoneName, AnimSequence);
		
		if (ConvertedBoneIndex == INDEX_NONE)
		{
			return;
		}

		AnimSequence->GetBoneTransform(BoneTransform, ConvertedBoneIndex, Time, true);

		OutTransform = OutTransform * BoneTransform;
	}

	const FTransform RootBoneTransform = AnimSequence->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0];
	OutTransform = OutTransform * RootBoneTransform;

	OutTransform.NormalizeRotation();
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutJointTransform, 
	const TArray<FBlendSampleData>& BlendSampleData, const TArray<FName>& BonesToRoot, const float Time)
{
	OutJointTransform = FTransform::Identity;

	if (BlendSampleData.Num() == 0 || BonesToRoot.Num() == 0)
	{
		return;
	}

	for (const FBlendSampleData& Sample : BlendSampleData)
	{
		if (!Sample.Animation)
		{
			continue;
		}

		const ScalarRegister VSampleWeight(Sample.GetWeight());

		FTransform AnimJointTransform = FTransform::Identity;
		for (const FName& BoneName : BonesToRoot)
		{
			FTransform BoneTransform;
			int32 ConvertedBoneIndex = ConvertBoneNameToAnimBoneId(BoneName, Sample.Animation);
			if (ConvertedBoneIndex == INDEX_NONE)
			{
				return;
			}

			Sample.Animation->GetBoneTransform(BoneTransform, ConvertedBoneIndex, Time, true);

			AnimJointTransform = AnimJointTransform * BoneTransform;
		}

		OutJointTransform.Accumulate(AnimJointTransform, VSampleWeight);
	}

	UAnimSequence* SourceAnim = BlendSampleData[0].Animation;
	if(SourceAnim)
	{
		const FTransform RootBoneTransform = BlendSampleData[0].Animation->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0];
		OutJointTransform = OutJointTransform * RootBoneTransform;
	}

	OutJointTransform.NormalizeRotation();
}

void FMMPreProcessUtils::GetJointTransform_RootRelative(FTransform& OutTransform, 
	UAnimComposite* AnimComposite, const TArray<FName>& BonesToRoot, const float Time)
{
	OutTransform = FTransform::Identity;

	if (!AnimComposite)
	{
		return;
	}

	float CumDuration = 0.0f;
	UAnimSequence* Sequence = nullptr;
	float NewTime = Time;
	//for (const FAnimSegment& AnimSegment : AnimComposite->AnimationTrack.AnimSegments)
	for(int32 i = 0; i < AnimComposite->AnimationTrack.AnimSegments.Num(); ++i)
	{
		const FAnimSegment& AnimSegment = AnimComposite->AnimationTrack.AnimSegments[i];
		const float Length = AnimSegment.AnimReference->GetPlayLength();

		if (Length + CumDuration > Time)
		{
			Sequence = Cast<UAnimSequence>(AnimSegment.AnimReference);
			break;
		}
		else
		{
			CumDuration += Length;
			NewTime -= Length;
		}
	}

	if (!Sequence)
	{
		return;
	}

	for (const FName& BoneName : BonesToRoot)
	{
		FTransform BoneTransform;
		const int32 ConvertedBoneIndex = ConvertBoneNameToAnimBoneId(BoneName, Sequence);
		if (ConvertedBoneIndex == INDEX_NONE)
		{
			return;
		}

		Sequence->GetBoneTransform(BoneTransform, ConvertedBoneIndex, NewTime, true);

		OutTransform = OutTransform * BoneTransform;
	}

	
	const FTransform RootBoneTransform = Sequence->GetSkeleton()->GetReferenceSkeleton().GetRefBonePose()[0];
	OutTransform = OutTransform * RootBoneTransform;

	OutTransform.NormalizeRotation();
}

#endif