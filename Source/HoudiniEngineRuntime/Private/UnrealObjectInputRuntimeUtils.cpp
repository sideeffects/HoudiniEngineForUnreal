#include "UnrealObjectInputRuntimeUtils.h"

#include "HoudiniRuntimeSettings.h"
#include "UnrealObjectInputRuntimeTypes.h"
#include "UnrealObjectInputManager.h"

bool
FUnrealObjectInputRuntimeUtils::IsInputNodeDirty(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputManager const* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->IsDirty(InIdentifier);
}

bool
FUnrealObjectInputRuntimeUtils::MarkInputNodeAsDirty(const FUnrealObjectInputIdentifier& InIdentifier, const bool bInAlsoDirtyReferencedNodes)
{
	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->MarkAsDirty(InIdentifier, bInAlsoDirtyReferencedNodes);
}

bool
FUnrealObjectInputRuntimeUtils::ClearInputNodeDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier)
{
	if (!InIdentifier.IsValid())
		return false;

	FUnrealObjectInputManager* const Manager = FUnrealObjectInputManager::Get();
	if (!Manager)
		return false;

	return Manager->ClearDirtyFlag(InIdentifier);
}
