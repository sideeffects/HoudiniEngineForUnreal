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

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HoudiniPublicAPIObjectBase.generated.h"

/** An enum with values that determine if API errors are logged. */
UENUM(BlueprintType)
enum class EHoudiniPublicAPIErrorLogOption : uint8
{
	Invalid = 0,
	Auto = 1,
	Log = 2,
	NoLog = 3,
};

/**
 * Base class for API UObjects. Implements error logging: record and get a error messages for Houdini Public API objects. 
 */
UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniPublicAPIObjectBase : public UObject
{
	GENERATED_BODY()
	
public:
	UHoudiniPublicAPIObjectBase();
	
	/**
	 * Gets the last error message recorded.
	 * @param OutLastErrorMessage Set to the last error message recorded, or the empty string if there are no errors
	 * messages.
	 * @return true if there was an error message to set.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Errors")
	bool GetLastErrorMessage(FString& OutLastErrorMessage) const;

	/** Clear any error messages that have been set. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Errors")
	void ClearErrorMessages();

	/**
	 * Returns whether or not API errors are written to the log.
	 * @return true if API errors are logged as warnings, false if API errors are not logged.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Errors")
	bool IsLoggingErrors() const;
	FORCEINLINE
	virtual bool IsLoggingErrors_Implementation() const { return bIsLoggingErrors; }

	/**
	 * Sets whether or not API errors are written to the log.
	 * @param bInEnabled True if API errors should be logged as warnings, false if API errors should not logged.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Errors")
	void SetLoggingErrorsEnabled(const bool bInEnabled);
	FORCEINLINE
	virtual void SetLoggingErrorsEnabled_Implementation(const bool bInEnabled) { bIsLoggingErrors = bInEnabled; }

protected:
	/**
	 * Set an error message. This is recorded as the current/last error message.
	 * @param InErrorMessage The error message to set.
	 * @param InLoggingOption Determines the behavior around logging the error message. If
	 * EHoudiniPublicAPIErrorLogOption.Invalid or EHoudiniPublicAPIErrorLogOption.Auto then IsLoggingErrors() is used to
	 * determine if the error message should be logged. If EHoudiniPublicAPIErrorLogOption.Log, then the error message
	 * is logged as a warning. If EHoudiniPublicAPIErrorLogOption.NoLog then the error message is not logged.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Houdini Engine | Public API | Errors")
	void SetErrorMessage(
		const FString& InErrorMessage,
		const EHoudiniPublicAPIErrorLogOption InLoggingOption=EHoudiniPublicAPIErrorLogOption::Auto) const;

	/** The last error message that was set. */
	UPROPERTY()
	mutable FString LastErrorMessage;

	/** True if an errors have been set and not yet cleared. */
	UPROPERTY()
	mutable bool bHasError;

	/** If true, API errors logged with SetErrorMessage are written to the log as warnings by default. */
	UPROPERTY()
	bool bIsLoggingErrors;
};
