#pragma once

#include "HoudiniEngineRuntimePrivatePCH.h"

// Houdini Engine forward declarations
class FUnrealObjectInputIdentifier;


struct HOUDINIENGINERUNTIME_API FUnrealObjectInputRuntimeUtils
{
public:
	// Helper for checking if an input node is marked as dirty in the ref counted input system.
	static bool IsInputNodeDirty(const FUnrealObjectInputIdentifier& InIdentifier);

	// Helper for marking an input node, and optionally for reference nodes its referenced nodes, as dirty in the ref counted input system
	static bool MarkInputNodeAsDirty(const FUnrealObjectInputIdentifier& InIdentifier, bool bInAlsoDirtyReferencedNodes);

	// Helper for clearing the dirty flag an input node in the ref counted input system.
	static bool ClearInputNodeDirtyFlag(const FUnrealObjectInputIdentifier& InIdentifier);
};
