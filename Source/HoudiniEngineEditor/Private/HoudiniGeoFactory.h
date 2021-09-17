/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "HoudiniGeoFactory.generated.h"

class UClass;
class UObject;
class FFeedbackContext;

UCLASS(config = Editor)
class UHoudiniGeoFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	public:

		//
		// UFactory Interface
		//
		// Returns the name of the factory for menus
		virtual FText GetDisplayName() const override;
		// return true if it supports this class
		virtual bool DoesSupportClass(UClass * Class) override;
		//
		virtual UClass* ResolveSupportedClass() override;
		//
		//virtual UClass* ResolveSupportedClass() override;
		// Return true if we can import the file
		virtual bool FactoryCanImport(const FString& Filename) override;

		// Create a new object by importing it from a file name.
		virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName,
			EObjectFlags Flags, const FString& Filename, const TCHAR* Parms,
			FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

		//
		// FReimportHandler Interface
		//
		UObject* Import(UClass* InClass, UPackage* OuterObject, const FString & FileName, const FString & AbsoluteFilePath, EObjectFlags Flags, const bool& bReimport);

		// Check to see if we have a handler to manage the reimporting of the object
		virtual bool CanReimport(UObject * Obj, TArray< FString > & OutFilenames) override;

		// Sets the reimport path(s) for the specified object
		virtual void SetReimportPaths(UObject * Obj, const TArray< FString > & NewReimportPaths) override;
		// Attempt to reimport the specified object from its source
		virtual EReimportResult::Type Reimport(UObject * Obj) override;

		//void ShowImportOptionsWindow(TSharedPtr<SHoudiniGeoImportOptions>& Options, FString FilePath, const UHoudiniGeoImporter& Importer);

		virtual int32 GetPriority() const override;

		// 
		//
		//
};
