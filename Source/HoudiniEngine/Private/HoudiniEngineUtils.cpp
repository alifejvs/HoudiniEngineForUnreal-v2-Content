/*
* Copyright (c) <2018> Side Effects Software Inc.
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

#include "HoudiniEngineUtils.h"

#if PLATFORM_WINDOWS
	#include "Windows/WindowsHWrapper.h"

	// Of course, Windows defines its own GetGeoInfo,
	// So we need to undefine that before including HoudiniApi.h to avoid collision...
	#ifdef GetGeoInfo
		#undef GetGeoInfo
	#endif
#endif

#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniAsset.h"
#include "HoudiniAssetActor.h"
#include "HoudiniEngineString.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniGenericAttribute.h"
#include "HoudiniInput.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniParameter.h"
#include "HoudiniEngineRuntimeUtils.h"

#include "HAPI/HAPI_Version.h"

#include "Misc/Paths.h"
#include "Editor/EditorEngine.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMeshSocket.h"
#include "Async/Async.h"
#include "BlueprintEditor.h"
#include "Toolkits/AssetEditorManager.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/MetaData.h"
#include "RawMesh.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
//#include "Kismet/BlueprintEditor.h"

#include <vector>

// HAPI_Result strings
const FString kResultStringSuccess(TEXT("Success"));
const FString kResultStringFailure(TEXT("Generic Failure"));
const FString kResultStringAlreadyInitialized(TEXT("Already Initialized"));
const FString kResultStringNotInitialized(TEXT("Not Initialized"));
const FString kResultStringCannotLoadFile(TEXT("Unable to Load File"));
const FString kResultStringParmSetFailed(TEXT("Failed Setting Parameter"));
const FString kResultStringInvalidArgument(TEXT("Invalid Argument"));
const FString kResultStringCannotLoadGeo(TEXT("Uneable to Load Geometry"));
const FString kResultStringCannotGeneratePreset(TEXT("Uneable to Generate Preset"));
const FString kResultStringCannotLoadPreset(TEXT("Uneable to Load Preset"));
const FString kResultStringAssetDefAlrealdyLoaded(TEXT("Asset definition already loaded"));
const FString kResultStringNoLicenseFound(TEXT("No License Found"));
const FString kResultStringDisallowedNCLicenseFound(TEXT("Disallowed Non Commercial License found"));
const FString kResultStringDisallowedNCAssetWithCLicense(TEXT("Disallowed Non Commercial Asset With Commercial License"));
const FString kResultStringDisallowedNCAssetWithLCLicense(TEXT("Disallowed Non Commercial Asset With Limited Commercial License"));
const FString kResultStringDisallowedLCAssetWithCLicense(TEXT("Disallowed Limited Commercial Asset With Commercial License"));
const FString kResultStringDisallowedHengineIndieWith3PartyPlugin(TEXT("Disallowed Houdini Engine Indie With 3rd Party Plugin"));
const FString kResultStringAssetInvalid(TEXT("Invalid Asset"));
const FString kResultStringNodeInvalid(TEXT("Invalid Node"));
const FString kResultStringUserInterrupted(TEXT("User Interrupt"));
const FString kResultStringInvalidSession(TEXT("Invalid Session"));
const FString kResultStringUnknowFailure(TEXT("Unknown Failure"));

const int32
FHoudiniEngineUtils::PackageGUIDComponentNameLength = 12;

const int32
FHoudiniEngineUtils::PackageGUIDItemNameLength = 8;

const FString
FHoudiniEngineUtils::GetErrorDescription(HAPI_Result Result)
{
	if (Result == HAPI_RESULT_SUCCESS)
	{
		return kResultStringSuccess;
	}
	else
	{
		switch (Result)
		{
		case HAPI_RESULT_FAILURE:
		{
			return kResultStringFailure;
		}

		case HAPI_RESULT_ALREADY_INITIALIZED:
		{
			return kResultStringAlreadyInitialized;
		}

		case HAPI_RESULT_NOT_INITIALIZED:
		{
			return kResultStringNotInitialized;
		}

		case HAPI_RESULT_CANT_LOADFILE:
		{
			return kResultStringCannotLoadFile;
		}

		case HAPI_RESULT_PARM_SET_FAILED:
		{
			return kResultStringParmSetFailed;
		}

		case HAPI_RESULT_INVALID_ARGUMENT:
		{
			return kResultStringInvalidArgument;
		}

		case HAPI_RESULT_CANT_LOAD_GEO:
		{
			return kResultStringCannotLoadGeo;
		}

		case HAPI_RESULT_CANT_GENERATE_PRESET:
		{
			return kResultStringCannotGeneratePreset;
		}

		case HAPI_RESULT_CANT_LOAD_PRESET:
		{
			return kResultStringCannotLoadPreset;
		}

		case HAPI_RESULT_ASSET_DEF_ALREADY_LOADED:
		{
			return kResultStringAssetDefAlrealdyLoaded;
		}

		case HAPI_RESULT_NO_LICENSE_FOUND:
		{
			return kResultStringNoLicenseFound;
		}

		case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND:
		{
			return kResultStringDisallowedNCLicenseFound;
		}

		case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE:
		{
			return kResultStringDisallowedNCAssetWithCLicense;
		}

		case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE:
		{
			return kResultStringDisallowedNCAssetWithLCLicense;
		}

		case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE:
		{
			return kResultStringDisallowedLCAssetWithCLicense;
		}

		case HAPI_RESULT_DISALLOWED_HENGINEINDIE_W_3PARTY_PLUGIN:
		{
			return kResultStringDisallowedHengineIndieWith3PartyPlugin;
		}

		case HAPI_RESULT_ASSET_INVALID:
		{
			return kResultStringAssetInvalid;
		}

		case HAPI_RESULT_NODE_INVALID:
		{
			return kResultStringNodeInvalid;
		}

		case HAPI_RESULT_USER_INTERRUPTED:
		{
			return kResultStringUserInterrupted;
		}

		case HAPI_RESULT_INVALID_SESSION:
		{
			return kResultStringInvalidSession;
		}

		default:
		{
			return kResultStringUnknowFailure;
		}
		};
	}
}

const FString
FHoudiniEngineUtils::GetStatusString(HAPI_StatusType status_type, HAPI_StatusVerbosity verbosity)
{
	const HAPI_Session* SessionPtr = FHoudiniEngine::Get().GetSession();
	if (!SessionPtr)
	{
		// No valid session
		return FString(TEXT("No valid Houdini Engine session."));
	}

	int32 StatusBufferLength = 0;
	HAPI_Result Result = FHoudiniApi::GetStatusStringBufLength(
		SessionPtr, status_type, verbosity, &StatusBufferLength);

	if (Result == HAPI_RESULT_INVALID_SESSION)
	{
		// Let FHoudiniEngine know that the sesion is now invalid to "Stop" the invalid session
		// and clean things up
		FHoudiniEngine::Get().OnSessionLost();
	}

	if (StatusBufferLength > 0)
	{
		TArray< char > StatusStringBuffer;
		StatusStringBuffer.SetNumZeroed(StatusBufferLength);
		FHoudiniApi::GetStatusString(
			SessionPtr, status_type, &StatusStringBuffer[0], StatusBufferLength);

		return FString(UTF8_TO_TCHAR(&StatusStringBuffer[0]));
	}

	return FString(TEXT(""));
}

const FString
FHoudiniEngineUtils::GetCookResult()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_RESULT, HAPI_STATUSVERBOSITY_MESSAGES);
}

const FString
FHoudiniEngineUtils::GetCookState()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_COOK_STATE, HAPI_STATUSVERBOSITY_ERRORS);
}

const FString
FHoudiniEngineUtils::GetErrorDescription()
{
	return FHoudiniEngineUtils::GetStatusString(HAPI_STATUS_CALL_RESULT, HAPI_STATUSVERBOSITY_ERRORS);
}

const FString
FHoudiniEngineUtils::GetNodeErrorsWarningsAndMessages(const HAPI_NodeId& InNodeId)
{
	int32 NodeErrorLength = 0;
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::ComposeNodeCookResult(
		FHoudiniEngine::Get().GetSession(), 
		InNodeId, HAPI_StatusVerbosity::HAPI_STATUSVERBOSITY_ALL, &NodeErrorLength))
	{
		NodeErrorLength = 0;
	}

	FString NodeError;
	if (NodeErrorLength > 0)
	{
		TArray<char> NodeErrorBuffer;
		NodeErrorBuffer.SetNumZeroed(NodeErrorLength);
		FHoudiniApi::GetComposedNodeCookResult(
			FHoudiniEngine::Get().GetSession(), &NodeErrorBuffer[0], NodeErrorLength);

		NodeError = FString(UTF8_TO_TCHAR(&NodeErrorBuffer[0]));
	}

	return NodeError;
}

const FString
FHoudiniEngineUtils::GetCookLog(TArray<UHoudiniAssetComponent*>& InHACs)
{
	FString CookLog;

	// Get fetch cook status.
	FString CookResult = FHoudiniEngineUtils::GetCookResult();
	if (!CookResult.IsEmpty())
		CookLog += TEXT("Cook Results:\n") + CookResult + TEXT("\n\n");

	// Add the cook state
	FString CookState = FHoudiniEngineUtils::GetCookState();
	if (!CookState.IsEmpty())
		CookLog += TEXT("Cook State:\n") + CookState + TEXT("\n\n");

	// Error Description
	FString Error = FHoudiniEngineUtils::GetErrorDescription();
	if (!Error.IsEmpty())
		CookLog += TEXT("Error Description:\n") + Error + TEXT("\n\n");

	// Iterates on all the selected HAC and get their node errors
	for (auto& HAC : InHACs)
	{
		if (!HAC || HAC->IsPendingKill())
			continue;

		// Get the node errors, warnings and messages
		FString NodeErrors = FHoudiniEngineUtils::GetNodeErrorsWarningsAndMessages(HAC->GetAssetId());
		if (NodeErrors.IsEmpty())
			continue;

		CookLog += NodeErrors;
	}

	if (CookLog.IsEmpty())
	{
		// See if a failed HAPI initialization / invalid session is preventing us from getting the cook log
		if (!FHoudiniApi::IsHAPIInitialized())
		{
			CookLog += TEXT("\n\nThe Houdini Engine API Library (HAPI) has not been initialized properly.\n\n");
		}
		else
		{
			const HAPI_Session * SessionPtr = FHoudiniEngine::Get().GetSession();
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(SessionPtr))
			{
				CookLog += TEXT("\n\nThe current Houdini Engine Session is not valid.\n\n");
			}
			else if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsInitialized(SessionPtr))
			{
				CookLog += TEXT("\n\nThe current Houdini Engine Session has not been initialized properly.\n\n");
			}
		}

		if (!CookLog.IsEmpty())
		{
			CookLog += TEXT("Please try to restart the current Houdini Engine session via File > Restart Houdini Engine Session.\n\n");
		}
		else
		{
			CookLog = TEXT("\n\nThe cook log is empty...\n\n");
		}
	}

	return CookLog;
}

const FString
FHoudiniEngineUtils::GetAssetHelp(UHoudiniAssetComponent* HoudiniAssetComponent)
{
	FString HelpString = TEXT("");
	if (!HoudiniAssetComponent)
		return HelpString;

	HAPI_AssetInfo AssetInfo;
	FHoudiniApi::AssetInfo_Init(&AssetInfo);
	HAPI_NodeId AssetId = HoudiniAssetComponent->GetAssetId();
	if (AssetId < 0)
		return HelpString;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetId, &AssetInfo), HelpString);

	if (!FHoudiniEngineString::ToFString(AssetInfo.helpTextSH, HelpString))
		return HelpString;

	if (HelpString.IsEmpty())
		HelpString = TEXT("No Asset Help Found");

	return HelpString;
}

void
FHoudiniEngineUtils::ConvertUnrealString(const FString & UnrealString, std::string & String)
{
	String = TCHAR_TO_UTF8(*UnrealString);
}


FString
FHoudiniEngineUtils::ComputeVersionString(bool ExtraDigit)
{
	// Compute Houdini version string.
	FString HoudiniVersionString = FString::Printf(
		TEXT("%d.%d.%s%d"), HAPI_VERSION_HOUDINI_MAJOR,
		HAPI_VERSION_HOUDINI_MINOR,
		(ExtraDigit ? (TEXT("0.")) : TEXT("")),
		HAPI_VERSION_HOUDINI_BUILD);

	// If we have a patch version, we need to append it.
	if (HAPI_VERSION_HOUDINI_PATCH > 0)
		HoudiniVersionString = FString::Printf(TEXT("%s.%d"), *HoudiniVersionString, HAPI_VERSION_HOUDINI_PATCH);
	return HoudiniVersionString;
}

void *
FHoudiniEngineUtils::LoadLibHAPI(FString & StoredLibHAPILocation)
{
	FString HFSPath = TEXT("");
	void * HAPILibraryHandle = nullptr;

	// Look up HAPI_PATH environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
	FString HFS_ENV_VAR = FPlatformMisc::GetEnvironmentVariable(TEXT("HAPI_PATH"));
	if (!HFS_ENV_VAR.IsEmpty())
		HFSPath = HFS_ENV_VAR;

	// Look up environment variable; if it is not defined, 0 will stored in HFS_ENV_VARIABLE .
	HFS_ENV_VAR = FPlatformMisc::GetEnvironmentVariable(TEXT("HFS"));
	if (!HFS_ENV_VAR.IsEmpty())
		HFSPath = HFS_ENV_VAR;

	// Get platform specific name of libHAPI.
	FString LibHAPIName = FHoudiniEngineRuntimeUtils::GetLibHAPIName();

	// If we have a custom location specified through settings, attempt to use that.
	bool bCustomPathFound = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->bUseCustomHoudiniLocation)
	{
		// Create full path to libHAPI binary.
		FString CustomHoudiniLocationPath = HoudiniRuntimeSettings->CustomHoudiniLocation.Path;
		if (!CustomHoudiniLocationPath.IsEmpty())
		{
			// Convert path to absolute if it is relative.
			if (FPaths::IsRelative(CustomHoudiniLocationPath))
				CustomHoudiniLocationPath = FPaths::ConvertRelativePathToFull(CustomHoudiniLocationPath);

			FString LibHAPICustomPath = FString::Printf(TEXT("%s/%s"), *CustomHoudiniLocationPath, *LibHAPIName);

			if (FPaths::FileExists(LibHAPICustomPath))
			{
				HFSPath = CustomHoudiniLocationPath;
				bCustomPathFound = true;
			}
		}
	}

	// We have HFS environment variable defined (or custom location), attempt to load libHAPI from it.
	if (!HFSPath.IsEmpty())
	{
		if (!bCustomPathFound)
		{
#if PLATFORM_WINDOWS
			HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_WINDOWS);
#elif PLATFORM_MAC
			HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_MAC);
#elif PLATFORM_LINUX
			HFSPath += FString::Printf(TEXT("/%s"), HAPI_HFS_SUBFOLDER_LINUX);
#endif
		}

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);

		if (FPaths::FileExists(LibHAPIPath))
		{
			// libHAPI binary exists at specified location, attempt to load it.
			FPlatformProcess::PushDllDirectory(*HFSPath);
#if PLATFORM_WINDOWS
			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
#elif PLATFORM_MAC || PLATFORM_LINUX
			HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIPath);
#endif
			FPlatformProcess::PopDllDirectory(*HFSPath);

			// If library has been loaded successfully we can stop.
			if ( HAPILibraryHandle )
			{
				if (bCustomPathFound)
					HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from custom path %s"), *LibHAPIName, *HFSPath);
				else
					HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from HFS environment path %s"), *LibHAPIName, *HFSPath);

				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
	}

	// Otherwise, we will attempt to detect Houdini installation.
	FString HoudiniLocation = TEXT(HOUDINI_ENGINE_HFS_PATH);
	FString LibHAPIPath;

	// Compute Houdini version string.
	FString HoudiniVersionString = ComputeVersionString(false);

#if PLATFORM_WINDOWS

	// On Windows, we have also hardcoded HFS path in plugin configuration file; attempt to load from it.
	HFSPath = FString::Printf(TEXT("%s/%s"), *HoudiniLocation, HAPI_HFS_SUBFOLDER_WINDOWS);

	// Create full path to libHAPI binary.
	LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, *LibHAPIName);

	if (FPaths::FileExists(LibHAPIPath))
	{
		FPlatformProcess::PushDllDirectory(*HFSPath);
		HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIName);
		FPlatformProcess::PopDllDirectory(*HFSPath);

		if (HAPILibraryHandle)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from Plugin defined HFS path %s"), *LibHAPIName, *HFSPath);
			StoredLibHAPILocation = HFSPath;
			return HAPILibraryHandle;
		}
	}

	// As a second attempt, on Windows, we try to look up location of Houdini Engine in the registry.
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini Engine"), StoredLibHAPILocation, false);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// As a third attempt, we try to look up location of Houdini installation (not Houdini Engine) in the registry.
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini"), StoredLibHAPILocation, false);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// Do similar registry lookups for the 32 bits registry
	// Look for the Houdini Engine registry install path
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini Engine"), StoredLibHAPILocation, true);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// ... and for the Houdini registry install path
	HAPILibraryHandle = FHoudiniEngineUtils::LocateLibHAPIInRegistry(
		TEXT("Houdini"), StoredLibHAPILocation, true);
	if (HAPILibraryHandle)
		return HAPILibraryHandle;

	// Finally, try to load from a hardcoded program files path.
	HoudiniLocation = FString::Printf(
		TEXT("C:\\Program Files\\Side Effects Software\\Houdini %s\\%s"), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_WINDOWS);

#else

#   if PLATFORM_MAC

	// Attempt to load from standard Mac OS X installation.
	HoudiniLocation = FString::Printf(
		TEXT("/Applications/Houdini/Houdini%s/Frameworks/Houdini.framework/Versions/Current/Libraries"), *HoudiniVersionString);

	// Fallback in case the previous one doesnt exist
	if (!FPaths::DirectoryExists(HoudiniLocation))
		HoudiniLocation = FString::Printf(
			TEXT("/Applications/Houdini/Houdini%s/Frameworks/Houdini.framework/Versions/%s/Libraries"), *HoudiniVersionString, *HoudiniVersionString);

	// Fallback in case we're using the steam version
	if (!FPaths::DirectoryExists(HoudiniLocation))
		HoudiniLocation = FString::Printf(
			TEXT("/Applications/Houdini/HoudiniIndieSteam/Frameworks/Houdini.framework/Versions/Current/Libraries"));

	// Backup Fallback in case we're using the steam version
	// (this could probably be removed as paths have changed)
	if (!FPaths::DirectoryExists(HoudiniLocation))
		HoudiniLocation = FString::Printf(
			TEXT("/Users/Shared/Houdini/HoudiniIndieSteam/Frameworks/Houdini.framework/Versions/Current/Libraries"));

#   elif PLATFORM_LINUX

	// Attempt to load from standard Linux installation.
	HoudiniLocation = FString::Printf(
		TEXT("/opt/hfs%s/%s"), *HoudiniVersionString, HAPI_HFS_SUBFOLDER_LINUX);

#   endif

#endif

	// Create full path to libHAPI binary.
	LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HoudiniLocation, *LibHAPIName);

	if (FPaths::FileExists(LibHAPIPath))
	{
		FPlatformProcess::PushDllDirectory(*HoudiniLocation);
		HAPILibraryHandle = FPlatformProcess::GetDllHandle(*LibHAPIPath);
		FPlatformProcess::PopDllDirectory(*HoudiniLocation);

		if (HAPILibraryHandle)
		{
			HOUDINI_LOG_MESSAGE(TEXT("Loaded %s from expected installation %s"), *LibHAPIName, *HoudiniLocation);
			StoredLibHAPILocation = HoudiniLocation;
			return HAPILibraryHandle;
		}
	}

	StoredLibHAPILocation = TEXT("");
	return HAPILibraryHandle;
}

bool
FHoudiniEngineUtils::IsInitialized()
{
	if (!FHoudiniApi::IsHAPIInitialized())
		return false;

	const HAPI_Session * SessionPtr = FHoudiniEngine::Get().GetSession();
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsSessionValid(SessionPtr))
		return false;

	return (FHoudiniApi::IsInitialized(SessionPtr) == HAPI_RESULT_SUCCESS);
}

bool
FHoudiniEngineUtils::IsHoudiniNodeValid(const HAPI_NodeId& NodeId)
{
	if (NodeId < 0)
		return false;

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	bool ValidationAnswer = 0;

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
	{
		return false;
	}

	if (HAPI_RESULT_SUCCESS != FHoudiniApi::IsNodeValid(
		FHoudiniEngine::Get().GetSession(), NodeId,
		NodeInfo.uniqueHoudiniNodeId, &ValidationAnswer))
	{
		return false;
	}

	return ValidationAnswer;
}

bool
FHoudiniEngineUtils::HapiDisconnectAsset(HAPI_NodeId HostAssetId, int32 InputIndex)
{
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::DisconnectNodeInput(
		FHoudiniEngine::Get().GetSession(), HostAssetId, InputIndex), false);

	return true;
}

bool
FHoudiniEngineUtils::DestroyHoudiniAsset(const HAPI_NodeId& AssetId)
{
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::DeleteNode(
		FHoudiniEngine::Get().GetSession(), AssetId))
	{
		return true;
	}

	return false;
}

#if PLATFORM_WINDOWS
void *
FHoudiniEngineUtils::LocateLibHAPIInRegistry(
	const FString & HoudiniInstallationType,
	FString & StoredLibHAPILocation,
	bool LookIn32bitRegistry)
{
	auto FindDll = [&](const FString& InHoudiniInstallationPath)
	{
		FString HFSPath = FString::Printf(TEXT("%s/%s"), *InHoudiniInstallationPath, HAPI_HFS_SUBFOLDER_WINDOWS);

		// Create full path to libHAPI binary.
		FString LibHAPIPath = FString::Printf(TEXT("%s/%s"), *HFSPath, HAPI_LIB_OBJECT_WINDOWS);

		if (FPaths::FileExists(LibHAPIPath))
		{
			FPlatformProcess::PushDllDirectory(*HFSPath);
			void* HAPILibraryHandle = FPlatformProcess::GetDllHandle(HAPI_LIB_OBJECT_WINDOWS);
			FPlatformProcess::PopDllDirectory(*HFSPath);

			if (HAPILibraryHandle)
			{
				HOUDINI_LOG_MESSAGE(
					TEXT("Loaded %s from Registry path %s"), HAPI_LIB_OBJECT_WINDOWS,
					*HFSPath);

				StoredLibHAPILocation = HFSPath;
				return HAPILibraryHandle;
			}
		}
		return (void*)0;
	};

	FString HoudiniInstallationPath;
	FString HoudiniVersionString = ComputeVersionString(true);
	FString RegistryKey = FString::Printf(
		TEXT("Software\\%sSide Effects Software\\%s"),
		(LookIn32bitRegistry ? TEXT("WOW6432Node\\") : TEXT("")), *HoudiniInstallationType);

	if (FWindowsPlatformMisc::QueryRegKey(
		HKEY_LOCAL_MACHINE, *RegistryKey, *HoudiniVersionString, HoudiniInstallationPath))
	{
		FPaths::NormalizeDirectoryName(HoudiniInstallationPath);
		return FindDll(HoudiniInstallationPath);
	}

	return nullptr;
}
#endif

bool
FHoudiniEngineUtils::LoadHoudiniAsset(UHoudiniAsset * HoudiniAsset, HAPI_AssetLibraryId & OutAssetLibraryId)
{
	OutAssetLibraryId = -1;

	if (!HoudiniAsset || HoudiniAsset->IsPendingKill())
		return false;

	if (!FHoudiniEngineUtils::IsInitialized())
		return false;

	// Get the HDA's file path
	// We need to convert relative file path to absolute
	FString AssetFileName = HoudiniAsset->GetAssetFileName();
	if (FPaths::IsRelative(AssetFileName))
		AssetFileName = FPaths::ConvertRelativePathToFull(AssetFileName);

	// We need to modify the file name for expanded .hdas
	FString FileExtension = FPaths::GetExtension(AssetFileName);
	if (FileExtension.Compare(TEXT("hdalibrary"), ESearchCase::IgnoreCase) == 0)
	{
		// the .hda directory is what we should be loading
		AssetFileName = FPaths::GetPath(AssetFileName);
	}

	// If the hda file exists, we can simply load it directly the file
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if ( !AssetFileName.IsEmpty() )
	{
		if ( FPaths::FileExists(AssetFileName)
			|| (HoudiniAsset->IsExpandedHDA() && FPaths::DirectoryExists(AssetFileName) ) )
		{
			// Load the asset from file.
			std::string AssetFileNamePlain;
			FHoudiniEngineUtils::ConvertUnrealString(AssetFileName, AssetFileNamePlain);
			Result = FHoudiniApi::LoadAssetLibraryFromFile(
				FHoudiniEngine::Get().GetSession(), AssetFileNamePlain.c_str(), true, &OutAssetLibraryId);
		}
	}

	// If loading from file failed, try to load using the memory copy
	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Expanded hdas cannot be loaded from  Memory
		if (HoudiniAsset->IsExpandedHDA() || HoudiniAsset->GetAssetBytesCount() <= 0)
		{
			HOUDINI_LOG_ERROR(TEXT("Error loading Asset %s: source asset file not found and no memory copy available."), *AssetFileName);
			return false;
		}
		else
		{
			// Warn the user that we are loading from memory
			HOUDINI_LOG_WARNING(TEXT("Asset %s, loading from Memory: source asset file not found."), *AssetFileName);

			// Otherwise we will try to load from buffer we've cached.
			Result = FHoudiniApi::LoadAssetLibraryFromMemory(
				FHoudiniEngine::Get().GetSession(),
				reinterpret_cast<const char *>(HoudiniAsset->GetAssetBytes()),
				HoudiniAsset->GetAssetBytesCount(), true, &OutAssetLibraryId);
		}
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_MESSAGE(TEXT("Error loading asset library for %s: %s"), *AssetFileName, *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	return true;
}

bool
FHoudiniEngineUtils::GetSubAssetNames(
	const HAPI_AssetLibraryId& AssetLibraryId,
	TArray< HAPI_StringHandle >& OutAssetNames)
{
	if (AssetLibraryId < 0)
		return false;

	int32 AssetCount = 0;
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	Result = FHoudiniApi::GetAvailableAssetCount(FHoudiniEngine::Get().GetSession(), AssetLibraryId, &AssetCount);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_ERROR(TEXT("Error getting asset count: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	if (AssetCount <= 0)
	{
		HOUDINI_LOG_ERROR(TEXT("Could not find an asset."));
		return false;
	}

	OutAssetNames.SetNumUninitialized(AssetCount);
	Result = FHoudiniApi::GetAvailableAssets(FHoudiniEngine::Get().GetSession(), AssetLibraryId, &OutAssetNames[0], AssetCount);
	if (Result != HAPI_RESULT_SUCCESS)
	{
		HOUDINI_LOG_ERROR(TEXT("Unable to retrieve sub asset names: %s"), *FHoudiniEngineUtils::GetErrorDescription());
		return false;
	}

	if (!AssetCount)
	{
		HOUDINI_LOG_ERROR(TEXT("No assets found"));
		return false;
	}

	return true;
}


bool
FHoudiniEngineUtils::OpenSubassetSelectionWindow(TArray<HAPI_StringHandle>& AssetNames, HAPI_StringHandle& OutPickedAssetName )
{
	OutPickedAssetName = -1;

	if (AssetNames.Num() <= 0)
		return false;

	// Default to the first asset
	OutPickedAssetName = AssetNames[0];
	/*
#if WITH_EDITOR
	// Present the user with a dialog for choosing which asset to instantiate.
	TSharedPtr<SWindow> ParentWindow;	
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		// Check if the main frame is loaded. When using the old main frame it may not be.
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (!ParentWindow.IsValid())
	{
		return false;
	}		

	TSharedPtr<SAssetSelectionWidget> AssetSelectionWidget;
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Select an asset to instantiate"))
		.ClientSize(FVector2D(640, 480))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.HasCloseButton(false);

	Window->SetContent(SAssignNew(AssetSelectionWidget, SAssetSelectionWidget)
		.WidgetWindow(Window)
		.AvailableAssetNames(AssetNames));

	if (!AssetSelectionWidget->IsValidWidget())
	{
		return false;
	}

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	int32 DialogPickedAssetName = AssetSelectionWidget->GetSelectedAssetName();
	if (DialogPickedAssetName != -1)
	{
		OutPickedAssetName = DialogPickedAssetName;
		return true;
	}
	else
	{
		return false;
	}
#endif
	*/

	return true;
}

/*
bool
FHoudiniEngineUtils::IsValidNodeId(HAPI_NodeId NodeId)
{
	return NodeId != -1;
}
*/

bool
FHoudiniEngineUtils::GetHoudiniAssetName(const HAPI_NodeId& AssetNodeId, FString & NameString)
{
	HAPI_AssetInfo AssetInfo;
	if (FHoudiniApi::GetAssetInfo(FHoudiniEngine::Get().GetSession(), AssetNodeId, &AssetInfo) == HAPI_RESULT_SUCCESS)
	{
		FHoudiniEngineString HoudiniEngineString(AssetInfo.nameSH);
		return HoudiniEngineString.ToFString(NameString);
	}

	return false;
}

bool
FHoudiniEngineUtils::GetAssetPreset(const HAPI_NodeId& AssetNodeId, TArray< char > & PresetBuffer)
{
	PresetBuffer.Empty();

	HAPI_NodeId NodeId;
	HAPI_AssetInfo AssetInfo;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), AssetNodeId, &AssetInfo))
	{
		NodeId = AssetInfo.nodeId;
	}
	else
		NodeId = AssetNodeId;

	int32 BufferLength = 0;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPresetBufLength(
		FHoudiniEngine::Get().GetSession(), NodeId,
		HAPI_PRESETTYPE_BINARY, NULL, &BufferLength), false);

	PresetBuffer.SetNumZeroed(BufferLength);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPreset(
		FHoudiniEngine::Get().GetSession(), NodeId,
		&PresetBuffer[0], PresetBuffer.Num()), false);

	return true;
}


bool
FHoudiniEngineUtils::HapiGetNodePath(const HAPI_NodeId& InNodeId, const HAPI_NodeId& InRelativeToNodeId, FString& OutPath)
{
	// Retrieve Path to the given Node, relative to the other given Node
	if ((InNodeId < 0) || (InRelativeToNodeId < 0))
		return false;

	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InNodeId))
		return false;

	HAPI_StringHandle StringHandle;
	if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodePath(
		FHoudiniEngine::Get().GetSession(),
		InNodeId, InRelativeToNodeId, &StringHandle))
	{
		if(FHoudiniEngineString::ToFString(StringHandle, OutPath))
		{
			return true;
		}
	}
	return false;
}

bool
FHoudiniEngineUtils::HapiGetNodePath(const FHoudiniGeoPartObject& InHGPO, FString& OutPath)
{
	// Do the HAPI query only on first-use
	if (!InHGPO.NodePath.IsEmpty())
		return true;

	FString NodePathTemp;
	if (InHGPO.AssetId == InHGPO.GeoId)
	{
		// This is a SOP asset, just return the asset name in this case
		HAPI_AssetInfo AssetInfo;
		FHoudiniApi::AssetInfo_Init(&AssetInfo);
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetAssetInfo(
			FHoudiniEngine::Get().GetSession(), InHGPO.AssetId, &AssetInfo))
		{
			HAPI_NodeInfo AssetNodeInfo;
			FHoudiniApi::NodeInfo_Init(&AssetNodeInfo);
			if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInfo(
				FHoudiniEngine::Get().GetSession(), AssetInfo.nodeId, &AssetNodeInfo))
			{				
				if (FHoudiniEngineString::ToFString(AssetNodeInfo.nameSH, NodePathTemp))
				{
					OutPath = FString::Printf(TEXT("%s_%d"), *NodePathTemp, InHGPO.PartId);
				}
			}
		}
	}
	else
	{
		// This is an OBJ asset, return the path to this geo relative to the asset
		if (FHoudiniEngineUtils::HapiGetNodePath(InHGPO.GeoId, InHGPO.AssetId, NodePathTemp))
		{
			OutPath = FString::Printf(TEXT("%s_%d"), *NodePathTemp, InHGPO.PartId);
		}
	}

	/*if (OutPath.IsEmpty())
	{
		OutPath = TEXT("Empty");
	}

	return NodePath;
	*/

	return !OutPath.IsEmpty();
}


bool
FHoudiniEngineUtils::HapiGetObjectInfos(const HAPI_NodeId& InNodeId, TArray<HAPI_ObjectInfo>& OutObjectInfos)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), 
		InNodeId, &NodeInfo), false);

	int32 ObjectCount = 0;
	if (NodeInfo.type == HAPI_NODETYPE_SOP)
	{
		ObjectCount = 1;
		OutObjectInfos.SetNumUninitialized(1);
		FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[0]));

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeInfo.parentId, &OutObjectInfos[0]), false);
	}
	else if (NodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeObjectList(
			FHoudiniEngine::Get().GetSession(), InNodeId, nullptr, &ObjectCount), false);

		if (ObjectCount <= 0)
		{
			ObjectCount = 1;
			OutObjectInfos.SetNumUninitialized(1);
			FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[0]));

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectInfo(
				FHoudiniEngine::Get().GetSession(), InNodeId,
				&OutObjectInfos[0]), false);
		}
		else
		{
			OutObjectInfos.SetNumUninitialized(ObjectCount);
			for (int32 Idx = 0; Idx < OutObjectInfos.Num(); Idx++)
				FHoudiniApi::ObjectInfo_Init(&(OutObjectInfos[0]));

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedObjectList(
				FHoudiniEngine::Get().GetSession(), InNodeId,
				&OutObjectInfos[0], 0, ObjectCount), false);
		}
	}
	else
		return false;

	return true;
}

bool
FHoudiniEngineUtils::HapiGetObjectTransforms(const HAPI_NodeId& InNodeId, TArray<HAPI_Transform>& OutObjectTransforms)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId,&NodeInfo), false);

	int32 ObjectCount = 1;
	OutObjectTransforms.SetNumUninitialized(1);
	FHoudiniApi::Transform_Init(&(OutObjectTransforms[0]));

	OutObjectTransforms[0].rotationQuaternion[3] = 1.0f;
	OutObjectTransforms[0].scale[0] = 1.0f;
	OutObjectTransforms[0].scale[1] = 1.0f;
	OutObjectTransforms[0].scale[2] = 1.0f;
	OutObjectTransforms[0].rstOrder = HAPI_SRT;

	if (NodeInfo.type == HAPI_NODETYPE_SOP)
	{
		// Do nothing. Identity transform will be used for the main parent object.
	}
	else if (NodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ComposeObjectList(
			FHoudiniEngine::Get().GetSession(), 
			InNodeId, nullptr, &ObjectCount), false);

		if (ObjectCount <= 0)
		{
			// Do nothing. Identity transform will be used for the main asset object.
		}
		else
		{
			OutObjectTransforms.SetNumUninitialized(ObjectCount);
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetComposedObjectTransforms(
				FHoudiniEngine::Get().GetSession(),
				InNodeId, HAPI_SRT, &OutObjectTransforms[0], 0, ObjectCount), false);
		}
	}
	else
		return false;

	return true;
}

bool
FHoudiniEngineUtils::HapiGetAssetTransform(const HAPI_NodeId& InNodeId, FTransform& OutTransform)
{
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), InNodeId,
		&NodeInfo), false);

	HAPI_Transform HapiTransform;
	FHoudiniApi::Transform_Init(&HapiTransform);

	if (NodeInfo.type == HAPI_NODETYPE_SOP)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectTransform(
			FHoudiniEngine::Get().GetSession(), 
			NodeInfo.parentId, -1, HAPI_SRT, &HapiTransform), false);
	}
	else if (NodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetObjectTransform(
			FHoudiniEngine::Get().GetSession(),
			InNodeId, -1, HAPI_SRT, &HapiTransform), false);
	}
	else
		return false;

	// Convert HAPI transform to Unreal one.
	FHoudiniEngineUtils::TranslateHapiTransform(HapiTransform, OutTransform);

	return true;
}

void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_Transform & HapiTransform, FTransform & UnrealTransform)
{
	if ( HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM )
	{
		// Swap Y/Z, invert W
		FQuat ObjectRotation(
			HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[2],
			HapiTransform.rotationQuaternion[1], -HapiTransform.rotationQuaternion[3]);		

		// Swap Y/Z and scale
		FVector ObjectTranslation(HapiTransform.position[0], HapiTransform.position[2], HapiTransform.position[1]);
		ObjectTranslation *= HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[2], HapiTransform.scale[1]);
		Swap(ObjectScale3D.Y, ObjectScale3D.Z);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
	else
	{
		FQuat ObjectRotation(
			HapiTransform.rotationQuaternion[0], HapiTransform.rotationQuaternion[1],
			HapiTransform.rotationQuaternion[2], HapiTransform.rotationQuaternion[3]);

		FVector ObjectTranslation(
			HapiTransform.position[0], HapiTransform.position[1], HapiTransform.position[2]);
		ObjectTranslation *= HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		FVector ObjectScale3D(HapiTransform.scale[0], HapiTransform.scale[1], HapiTransform.scale[2]);

		UnrealTransform.SetComponents(ObjectRotation, ObjectTranslation, ObjectScale3D);
	}
}

void
FHoudiniEngineUtils::TranslateHapiTransform(const HAPI_TransformEuler & HapiTransformEuler, FTransform & UnrealTransform)
{
	float HapiMatrix[16];
	FHoudiniApi::ConvertTransformEulerToMatrix(FHoudiniEngine::Get().GetSession(), &HapiTransformEuler, HapiMatrix);

	HAPI_Transform HapiTransformQuat;
	FMemory::Memzero< HAPI_Transform >(HapiTransformQuat);
	FHoudiniApi::ConvertMatrixToQuat(FHoudiniEngine::Get().GetSession(), HapiMatrix, HAPI_SRT, &HapiTransformQuat);

	FHoudiniEngineUtils::TranslateHapiTransform(HapiTransformQuat, UnrealTransform);
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(const FTransform & UnrealTransform, HAPI_Transform & HapiTransform)
{
	FMemory::Memzero< HAPI_Transform >(HapiTransform);
	HapiTransform.rstOrder = HAPI_SRT;

	FQuat UnrealRotation = UnrealTransform.GetRotation();
	FVector UnrealTranslation = UnrealTransform.GetTranslation();
	FVector UnrealScale = UnrealTransform.GetScale3D();

	if (HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM)
	{
		// Swap Y/Z, invert XYZ
		HapiTransform.rotationQuaternion[0] = -UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = -UnrealRotation.Z;
		HapiTransform.rotationQuaternion[2] = -UnrealRotation.Y;
		HapiTransform.rotationQuaternion[3] = UnrealRotation.W;

		// Swap Y/Z, scale
		HapiTransform.position[0] = UnrealTranslation.X / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransform.position[1] = UnrealTranslation.Z / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransform.position[2] = UnrealTranslation.Y / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		HapiTransform.scale[0] = UnrealScale.X;
		HapiTransform.scale[1] = UnrealScale.Z;
		HapiTransform.scale[2] = UnrealScale.Y;
	}
	else
	{
		HapiTransform.rotationQuaternion[0] = UnrealRotation.X;
		HapiTransform.rotationQuaternion[1] = UnrealRotation.Y;
		HapiTransform.rotationQuaternion[2] = UnrealRotation.Z;
		HapiTransform.rotationQuaternion[3] = UnrealRotation.W;

		HapiTransform.position[0] = UnrealTranslation.X;
		HapiTransform.position[1] = UnrealTranslation.Y;
		HapiTransform.position[2] = UnrealTranslation.Z;

		HapiTransform.scale[0] = UnrealScale.X;
		HapiTransform.scale[1] = UnrealScale.Y;
		HapiTransform.scale[2] = UnrealScale.Z;
	}
}

void
FHoudiniEngineUtils::TranslateUnrealTransform(
	const FTransform & UnrealTransform,
	HAPI_TransformEuler & HapiTransformEuler)
{
	FMemory::Memzero< HAPI_TransformEuler >(HapiTransformEuler);

	HapiTransformEuler.rstOrder = HAPI_SRT;
	HapiTransformEuler.rotationOrder = HAPI_XYZ;

	FQuat UnrealRotation = UnrealTransform.GetRotation();
	FVector UnrealTranslation = UnrealTransform.GetTranslation();
	FVector UnrealScale = UnrealTransform.GetScale3D();

	if (HAPI_UNREAL_CONVERT_COORDINATE_SYSTEM)
	{
		// switch the quaternion to Y-up, LHR by Swapping Y/Z and negating W
		Swap(UnrealRotation.Y, UnrealRotation.Z);
		UnrealRotation.W = -UnrealRotation.W;
		const FRotator Rotator = UnrealRotation.Rotator();

		// Negate roll and pitch since they are actually RHR
		HapiTransformEuler.rotationEuler[0] = -Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = -Rotator.Pitch;
		HapiTransformEuler.rotationEuler[2] = Rotator.Yaw;

		// Swap Y/Z, scale
		HapiTransformEuler.position[0] = UnrealTranslation.X / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransformEuler.position[1] = UnrealTranslation.Z / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;
		HapiTransformEuler.position[2] = UnrealTranslation.Y / HAPI_UNREAL_SCALE_FACTOR_TRANSLATION;

		// Swap Y/Z
		HapiTransformEuler.scale[0] = UnrealScale.X;
		HapiTransformEuler.scale[1] = UnrealScale.Z;
		HapiTransformEuler.scale[2] = UnrealScale.Y;
	}
	else
	{
		const FRotator Rotator = UnrealRotation.Rotator();
		HapiTransformEuler.rotationEuler[0] = Rotator.Roll;
		HapiTransformEuler.rotationEuler[1] = Rotator.Yaw;
		HapiTransformEuler.rotationEuler[2] = Rotator.Pitch;

		HapiTransformEuler.position[0] = UnrealTranslation.X;
		HapiTransformEuler.position[1] = UnrealTranslation.Y;
		HapiTransformEuler.position[2] = UnrealTranslation.Z;

		HapiTransformEuler.scale[0] = UnrealScale.X;
		HapiTransformEuler.scale[1] = UnrealScale.Y;
		HapiTransformEuler.scale[2] = UnrealScale.Z;
	}
}

bool
FHoudiniEngineUtils::UploadHACTransform(UHoudiniAssetComponent* HAC)
{
	if (!HAC || !HAC->bUploadTransformsToHoudiniEngine)
		return false;

	// Indicates the HAC has been fully loaded
	// TODO: Check! (replaces fullyloaded)
	if (!HAC->IsFullyLoaded())
		return false;

	if (HAC->GetAssetCookCount() > 0 && HAC->GetAssetId() >= 0)
	{
		if (!FHoudiniEngineUtils::HapiSetAssetTransform(HAC->GetAssetId(), HAC->GetComponentTransform()))
			return false;
	}

	HAC->SetHasComponentTransformChanged(false);

	return true;
}

bool
FHoudiniEngineUtils::HapiSetAssetTransform(const HAPI_NodeId& AssetId, const FTransform & Transform)
{
	if (AssetId < 0)
		return false;

	// Translate Unreal transform to HAPI Euler one.
	HAPI_TransformEuler TransformEuler;
	FMemory::Memzero< HAPI_TransformEuler >(TransformEuler);
	FHoudiniEngineUtils::TranslateUnrealTransform(Transform, TransformEuler);

	// Get the NodeInfo
	HAPI_NodeInfo LocalAssetNodeInfo;
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
		FHoudiniEngine::Get().GetSession(), AssetId,
		&LocalAssetNodeInfo), false);

	if (LocalAssetNodeInfo.type == HAPI_NODETYPE_SOP)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(),
			LocalAssetNodeInfo.parentId,
			&TransformEuler), false);
	}
	else if (LocalAssetNodeInfo.type == HAPI_NODETYPE_OBJ)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetObjectTransform(
			FHoudiniEngine::Get().GetSession(),
			AssetId, &TransformEuler), false);
	}
	else
		return false;

	return true;
}

HAPI_NodeId
FHoudiniEngineUtils::HapiGetParentNodeId(const HAPI_NodeId& NodeId)
{
	HAPI_NodeId ParentId = -1;
	if (NodeId >= 0)
	{
		HAPI_NodeInfo NodeInfo;
		if (HAPI_RESULT_SUCCESS == FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo))
			ParentId = NodeInfo.parentId;
	}

	return ParentId;
}


// Assign a unique Actor Label if needed
void
FHoudiniEngineUtils::AssignUniqueActorLabelIfNeeded(UHoudiniAssetComponent* HAC)
{
	if (!HAC || HAC->IsPendingKill())
		return;

	// TODO: Necessary??

#if WITH_EDITOR
	HAPI_NodeId AssetId = HAC->GetAssetId();
	if (AssetId < 0)
		return;

	AActor* OwnerActor = HAC->GetOwner();
	if (!OwnerActor)
		return;

	if (!OwnerActor->GetName().StartsWith(AHoudiniAssetActor::StaticClass()->GetName()))
		return;

	// Assign unique actor label based on asset name if it seems to have not been renamed already
	FString UniqueName;
	if (FHoudiniEngineUtils::GetHoudiniAssetName(AssetId, UniqueName))
		FActorLabelUtilities::SetActorLabelUnique(OwnerActor, UniqueName);
#endif
}

bool
FHoudiniEngineUtils::GetLicenseType(FString & LicenseType)
{
	LicenseType = TEXT("");
	HAPI_License LicenseTypeValue = HAPI_LICENSE_NONE;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetSessionEnvInt(
		FHoudiniEngine::Get().GetSession(), HAPI_SESSIONENVINT_LICENSE,
		(int32 *)&LicenseTypeValue), false);

	switch (LicenseTypeValue)
	{
	case HAPI_LICENSE_NONE:
	{
		LicenseType = TEXT("No License Acquired");
		break;
	}

	case HAPI_LICENSE_HOUDINI_ENGINE:
	{
		LicenseType = TEXT("Houdini Engine");
		break;
	}

	case HAPI_LICENSE_HOUDINI:
	{
		LicenseType = TEXT("Houdini");
		break;
	}

	case HAPI_LICENSE_HOUDINI_FX:
	{
		LicenseType = TEXT("Houdini FX");
		break;
	}

	case HAPI_LICENSE_HOUDINI_ENGINE_INDIE:
	{
		LicenseType = TEXT("Houdini Engine Indie");
		break;
	}

	case HAPI_LICENSE_HOUDINI_INDIE:
	{
		LicenseType = TEXT("Houdini Indie");
		break;
	}

	case HAPI_LICENSE_MAX:
	default:
	{
		return false;
	}
	}

	return true;
}

// Check if the Houdini asset component (or parent HAC of a parameter) is being cooked
bool
FHoudiniEngineUtils::IsHoudiniAssetComponentCooking(UObject* InObj) 
{
	if (!InObj)
		return false;

	UHoudiniAssetComponent* HoudiniAssetComponent = nullptr;

	if (InObj->IsA<UHoudiniAssetComponent>()) 
	{
		HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InObj);
	}
	else if (InObj->IsA<UHoudiniParameter>())
	{
		UHoudiniParameter* Parameter = Cast<UHoudiniParameter>(InObj);
		if (!Parameter)
			return false;

		HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(Parameter->GetOuter());
	}

	if (!HoudiniAssetComponent)
		return false;

	EHoudiniAssetState AssetState = HoudiniAssetComponent->GetAssetState();

	return AssetState >= EHoudiniAssetState::PreCook && AssetState <= EHoudiniAssetState::PostCook;
}

void
FHoudiniEngineUtils::UpdateEditorProperties(UObject* InObjectToUpdate, const bool& InForceFullUpdate)
{
	TArray<UObject*> ObjectsToUpdate;
	ObjectsToUpdate.Add(InObjectToUpdate);

	if (!IsInGameThread())
	{
		// We need to be in the game thread to trigger editor properties update
		AsyncTask(ENamedThreads::GameThread, [ObjectsToUpdate, InForceFullUpdate]()
		{
			FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
		});
	}
	else
	{
		// We're in the game thread, no need  for an async task
		FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
	}
}

void
FHoudiniEngineUtils::UpdateEditorProperties(TArray<UObject*> ObjectsToUpdate, const bool& InForceFullUpdate)
{
	if (!IsInGameThread())
	{
		// We need to be in the game thread to trigger editor properties update
		AsyncTask(ENamedThreads::GameThread, [ObjectsToUpdate, InForceFullUpdate]()
		{
			FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
		});
	}
	else
	{
		// We're in the game thread, no need  for an async task
		FHoudiniEngineUtils::UpdateEditorProperties_Internal(ObjectsToUpdate, InForceFullUpdate);
	}
}

void
FHoudiniEngineUtils::UpdateEditorProperties_Internal(TArray<UObject*> ObjectsToUpdate, const bool& bInForceFullUpdate)
{
#if WITH_EDITOR	
	if (!bInForceFullUpdate)
	{
		// bNeedFullUpdate is false only when small changes (parameters value) have been made
		// We do not reselect the actor to avoid loosing the currently selected parameter
		if(GUnrealEd)
			GUnrealEd->UpdateFloatingPropertyWindows();

		return;
	}

	// We now want to get all the components/actors owning the objects to update
	TArray<USceneComponent*> AllSceneComponents;
	for (auto CurrentObject : ObjectsToUpdate)
	{
		if (!CurrentObject || CurrentObject->IsPendingKill())
			continue;

		// In some case, the object itself is the component
		USceneComponent* SceneComp = Cast<USceneComponent>(CurrentObject);
		if (!SceneComp)
		{
			SceneComp = Cast<USceneComponent>(CurrentObject->GetOuter());
		}

		if (SceneComp && !SceneComp->IsPendingKill())
		{
			AllSceneComponents.Add(SceneComp);
			continue;
		}
	}

	TArray<AActor*> AllActors;
	for (auto CurrentSceneComp : AllSceneComponents)
	{
		if (!CurrentSceneComp || CurrentSceneComp->IsPendingKill())
			continue;

		AActor* Actor = CurrentSceneComp->GetOwner();
		if (Actor && !Actor->IsPendingKill())
			AllActors.Add(Actor);
	}

	// Updating the editor properties can be done in two ways, depending if we're in the BP editor or not
	// If we have a parent actor, we're not in the BP Editor, so update via the property editor module
	if (AllActors.Num() > 0)
	{
		// Get the property editor module
		FPropertyEditorModule& PropertyModule =
			FModuleManager::Get().GetModuleChecked< FPropertyEditorModule >("PropertyEditor");

		// This will actually force a refresh of all the details view
		//PropertyModule.NotifyCustomizationModuleChanged();

		TArray<UObject*> SelectedActors;
		for (auto Actor : AllActors)
		{
			if (Actor && Actor->IsSelected())
				SelectedActors.Add(Actor);
		}

		if (SelectedActors.Num() > 0)
		{
			PropertyModule.UpdatePropertyViews(SelectedActors);
		}

		// We want to iterate on all the details panel
		static const FName DetailsTabIdentifiers[] =
		{
			"LevelEditorSelectionDetails",
			"LevelEditorSelectionDetails2",
			"LevelEditorSelectionDetails3",
			"LevelEditorSelectionDetails4"
		};

		for (const FName& DetailsPanelName : DetailsTabIdentifiers)
		{
			// Locate the details panel.
			TSharedPtr<IDetailsView> DetailsView = PropertyModule.FindDetailView(DetailsPanelName);
			if (!DetailsView.IsValid())
			{
				// We have no details panel, nothing to update.
				continue;
			}

			// Get the selected actors for this details panels and check if one of ours belongs to it
			const TArray<TWeakObjectPtr<AActor>>& SelectedDetailActors = DetailsView->GetSelectedActors();
			bool bFoundActor = false;
			for (int32 ActorIdx = 0; ActorIdx < SelectedDetailActors.Num(); ActorIdx++)
			{
				TWeakObjectPtr<AActor> SelectedActor = SelectedDetailActors[ActorIdx];
				if (SelectedActor.IsValid() && AllActors.Contains(SelectedActor.Get()))
				{
					bFoundActor = true;
					break;
				}
			}
			
			// None of our actors belongs to this detail panel, no need to update it
			if (!bFoundActor)
				continue;

			// Refresh that details panels using its current selection
			TArray<UObject*> Selection;
			for (auto DetailsActor : SelectedDetailActors)
			{
				if (DetailsActor.IsValid())
					Selection.Add(DetailsActor.Get());
			}

			// Reset selected actors, force refresh and override the lock.
			DetailsView->SetObjects(SelectedActors, bInForceFullUpdate, true);

			if (GUnrealEd)
				GUnrealEd->UpdateFloatingPropertyWindows();
		}
	}
	else
	{
		// TODO: Proper BP UI UPDATE

		// For each component, find its BP Class owner
		for (auto CurComp : AllSceneComponents)
		{
			UBlueprintGeneratedClass* OwnerBPClass = Cast<UBlueprintGeneratedClass>(CurComp->GetOuter());
			if (!OwnerBPClass)
				return;

			/*
			FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(FAssetEditorManager::Get().FindEditorForAsset(OwnerBPClass->ClassGeneratedBy, false));
			if (!BlueprintEditor)
				return;
			*/

			// Close the mesh editor to prevent crashing. Reopen it after the mesh has been built.
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			FBlueprintEditor* BlueprintEditor = static_cast<FBlueprintEditor*>(AssetEditorSubsystem->FindEditorForAsset(OwnerBPClass->ClassGeneratedBy, false));
			if (!BlueprintEditor)
				return;

			BlueprintEditor->RefreshEditors();

			// Also somehow reselect ?
		}
	}

	/*
	// Reset the full update flag
	if (bNeedFullUpdate)
		HAC->SetEditorPropertiesNeedFullUpdate(false);
	*/

	return;
#endif
}

HAPI_Result
FHoudiniEngineUtils::SetAttributeStringData(
	const FString& InString,
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo)
{
	TArray<FString> StringArray;
	StringArray.Add(InString);

	return SetAttributeStringData(StringArray, InNodeId, InPartId, InAttributeName, InAttributeInfo);
}

HAPI_Result
FHoudiniEngineUtils::SetAttributeStringData(
	const TArray<FString>& InStringArray, 
	const HAPI_NodeId& InNodeId,
	const HAPI_PartId& InPartId,
	const FString& InAttributeName,
	const HAPI_AttributeInfo& InAttributeInfo )
{
	TArray<const char *> StringDataArray;
	for (auto CurrentString : InStringArray)
	{
		// Append the converted string to the string array
		StringDataArray.Add(FHoudiniEngineUtils::ExtractRawString(CurrentString));
	}

	// Set the attribute's string data
	HAPI_Result result = FHoudiniApi::SetAttributeStringData(
		FHoudiniEngine::Get().GetSession(), InNodeId, InPartId,
		TCHAR_TO_ANSI(*InAttributeName), &InAttributeInfo,
		StringDataArray.GetData(), 0, InAttributeInfo.count);

	// ExtractRawString allocates memory using malloc, free it!
	FreeRawStringMemory(StringDataArray);

	return result;
}

char *
FHoudiniEngineUtils::ExtractRawString(const FString& InString)
{
	if (InString.IsEmpty())
		return nullptr;

	std::string ConvertedString = TCHAR_TO_UTF8(*InString);

	// Allocate space for unique string.
	int32 UniqueStringBytes = ConvertedString.size() + 1;
	char * UniqueString = static_cast<char *>(FMemory::Malloc(UniqueStringBytes));

	FMemory::Memzero(UniqueString, UniqueStringBytes);
	FMemory::Memcpy(UniqueString, ConvertedString.c_str(), ConvertedString.size());

	return UniqueString;
}

void
FHoudiniEngineUtils::FreeRawStringMemory(const char*& InRawString)
{
	if (InRawString == nullptr)
		return;

	// Do not attempt to free empty strings!
	if (!InRawString[0])
		return;

	FMemory::Free((void*)InRawString);
	InRawString = nullptr;
}

void
FHoudiniEngineUtils::FreeRawStringMemory(TArray<const char*>& InRawStringArray)
{
	// ExtractRawString allocates memory using malloc, free it!
	for (auto CurrentStrPtr : InRawStringArray)
	{
		FreeRawStringMemory(CurrentStrPtr);
	}
	InRawStringArray.Empty();
}

bool
FHoudiniEngineUtils::AddHoudiniLogoToComponent(UHoudiniAssetComponent* HAC)
{
	if (!HAC || HAC->IsPendingKill())
		return false;

	// No need to add another component if we already show the logo
	if (FHoudiniEngineUtils::HasHoudiniLogo(HAC))
		return true;

	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	UStaticMeshComponent * HoudiniLogoSMC = NewObject< UStaticMeshComponent >(
		HAC, UStaticMeshComponent::StaticClass(), NAME_None, RF_Transactional);

	if (!HoudiniLogoSMC)
		return false;

	HoudiniLogoSMC->SetStaticMesh(HoudiniLogoSM);
	HoudiniLogoSMC->SetVisibility(true);
	// Attach created static mesh component to our Houdini component.
	HoudiniLogoSMC->AttachToComponent(HAC, FAttachmentTransformRules::KeepRelativeTransform);
	HoudiniLogoSMC->RegisterComponent();

	return true;
}

bool
FHoudiniEngineUtils::RemoveHoudiniLogoFromComponent(UHoudiniAssetComponent* HAC)
{
	if (!HAC || HAC->IsPendingKill())
		return false;

	// Get the Houdini Logo SM
	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : HAC->GetAttachChildren())
	{
		if (!CurrentSceneComp || CurrentSceneComp->IsPendingKill() || !CurrentSceneComp->IsA<UStaticMeshComponent>())
			continue;

		// Get the static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CurrentSceneComp);
		if (!SMC || SMC->IsPendingKill())
			continue;

		// Check if the SMC is the Houdini Logo
		if (SMC->GetStaticMesh() != HoudiniLogoSM)
			continue;

		SMC->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
		SMC->UnregisterComponent();
		SMC->DestroyComponent();

		return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::HasHoudiniLogo(UHoudiniAssetComponent* HAC)
{
	if (!HAC || HAC->IsPendingKill())
		return false;

	// Get the Houdini Logo SM
	UStaticMesh* HoudiniLogoSM = FHoudiniEngine::Get().GetHoudiniLogoStaticMesh().Get();
	if (!HoudiniLogoSM)
		return false;

	// Iterate on the HAC's component
	for (USceneComponent* CurrentSceneComp : HAC->GetAttachChildren())
	{
		if (!CurrentSceneComp || CurrentSceneComp->IsPendingKill() || !CurrentSceneComp->IsA<UStaticMeshComponent>())
			continue;

		// Get the static mesh component
		UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(CurrentSceneComp);
		if (!SMC || SMC->IsPendingKill())
			continue;

		// Check if the SMC is the Houdini Logo
		if (SMC->GetStaticMesh() == HoudiniLogoSM)
			return true;
	}

	return false;
}

int32
FHoudiniEngineUtils::HapiGetVertexListForGroup(
	const HAPI_NodeId& GeoId,
	const HAPI_PartInfo& PartInfo,
	const FString& GroupName,
	const TArray<int32>& FullVertexList,
	TArray<int32>& NewVertexList,
	TArray<int32>& AllVertexList,
	TArray<int32>& AllFaceList,
	TArray<int32>& AllGroupFaceIndices,
	int32& FirstValidVertex,
	int32& FirstValidPrim,
	const bool& isPackedPrim)
{
	int32 ProcessedWedges = 0;
	AllFaceList.Empty();
	FirstValidPrim = 0;
	FirstValidVertex = 0;
	NewVertexList.Init(-1, FullVertexList.Num());

	// Get the faces membership for this group
	bool bAllEquals = false;
	TArray<int32> PartGroupMembership;
	if (!FHoudiniEngineUtils::HapiGetGroupMembership(
		GeoId, PartInfo, HAPI_GROUPTYPE_PRIM, GroupName, PartGroupMembership, bAllEquals))
		return false;

	// Go through all primitives.
	for (int32 FaceIdx = 0; FaceIdx < PartGroupMembership.Num(); ++FaceIdx)
	{
		if (PartGroupMembership[FaceIdx] <= 0)
		{
			// The face is not in the group, skip
			continue;
		}
		
		// Add the face's index.
		AllFaceList.Add(FaceIdx);

		// Get the index of this face's vertices
		int32 FirstVertexIdx = FaceIdx * 3;
		int32 SecondVertexIdx = FirstVertexIdx + 1;
		int32 LastVertexIdx = FirstVertexIdx + 2;

		// This face is a member of specified group.
		// Add all 3 vertices
		if (FullVertexList.IsValidIndex(LastVertexIdx))
		{
			NewVertexList[FirstVertexIdx] = FullVertexList[FirstVertexIdx];
			NewVertexList[SecondVertexIdx] = FullVertexList[SecondVertexIdx];
			NewVertexList[LastVertexIdx] = FullVertexList[LastVertexIdx];
		}

		// Mark these vertex indices as used.
		if (AllVertexList.IsValidIndex(LastVertexIdx))
		{
			AllVertexList[FirstVertexIdx] = 1;
			AllVertexList[SecondVertexIdx] = 1;
			AllVertexList[LastVertexIdx] = 1;
		}

		// Mark this face as used.
		if (AllGroupFaceIndices.IsValidIndex(FaceIdx))
			AllGroupFaceIndices[FaceIdx] = 1;

		if (ProcessedWedges == 0)
		{
			// Keep track of the first valid vertex/face indices for this group
			// This will be useful later on when extracting attributes
			FirstValidVertex = FirstVertexIdx;
			FirstValidPrim = FaceIdx;
		}

		ProcessedWedges += 3;
	}

	return ProcessedWedges;
}

bool
FHoudiniEngineUtils::HapiGetGroupNames(
	const HAPI_NodeId& GeoId, const HAPI_PartId& PartId,
	const HAPI_GroupType& GroupType, const bool& isPackedPrim,
	TArray<FString>& OutGroupNames)
{
	int32 GroupCount = 0;
	if (!isPackedPrim)
	{
		// Get group count on the geo
		HAPI_GeoInfo GeoInfo;
		FHoudiniApi::GeoInfo_Init(&GeoInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
			FHoudiniEngine::Get().GetSession(), GeoId, &GeoInfo), false);

		if (GroupType == HAPI_GROUPTYPE_POINT)
			GroupCount = GeoInfo.pointGroupCount;
		else if (GroupType == HAPI_GROUPTYPE_PRIM)
			GroupCount = GeoInfo.primitiveGroupCount;
	}
	else
	{
		// We need the group count for this packed prim
		int32 PointGroupCount = 0, PrimGroupCount = 0;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupCountOnPackedInstancePart(
			FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PointGroupCount, &PrimGroupCount), false);

		if (GroupType == HAPI_GROUPTYPE_POINT)
			GroupCount = PointGroupCount;
		else if (GroupType == HAPI_GROUPTYPE_PRIM)
			GroupCount = PrimGroupCount;
	}

	if (GroupCount <= 0)
		return true;

	TArray<int32> GroupNameStringHandles;
	GroupNameStringHandles.SetNumZeroed(GroupCount);
	if (!isPackedPrim)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupNames(
			FHoudiniEngine::Get().GetSession(),
			GeoId, GroupType, &GroupNameStringHandles[0], GroupCount), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupNamesOnPackedInstancePart(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, GroupType, &GroupNameStringHandles[0], GroupCount), false);
	}

	OutGroupNames.SetNum(GroupCount);
	for (int32 NameIdx = 0; NameIdx < GroupCount; ++NameIdx)
	{
		FString CurrentGroupName = TEXT("");
		FHoudiniEngineString::ToFString(GroupNameStringHandles[NameIdx], CurrentGroupName);
		OutGroupNames[NameIdx] = CurrentGroupName;
	}

	return true;
}

bool
FHoudiniEngineUtils::HapiGetGroupMembership(
	const HAPI_NodeId& GeoId, const HAPI_PartInfo& PartInfo,
	const HAPI_GroupType& GroupType, const FString & GroupName,
	TArray<int32>& OutGroupMembership, bool& OutAllEquals)
{
	int32 ElementCount = (GroupType == HAPI_GROUPTYPE_POINT) ? PartInfo.pointCount : PartInfo.faceCount;	
	if (ElementCount < 1)
		return false;
	OutGroupMembership.SetNum(ElementCount);

	OutAllEquals = false;
	std::string ConvertedGroupName = TCHAR_TO_UTF8(*GroupName);
	if (!PartInfo.isInstanced)
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembership(
			FHoudiniEngine::Get().GetSession(), 
			GeoId, PartInfo.id, GroupType,ConvertedGroupName.c_str(),
			&OutAllEquals, &OutGroupMembership[0], 0, ElementCount), false);
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGroupMembershipOnPackedInstancePart(
			FHoudiniEngine::Get().GetSession(), GeoId, PartInfo.id, GroupType,
			ConvertedGroupName.c_str(), &OutAllEquals, &OutGroupMembership[0], 0, ElementCount), false);
	}

	return true;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& OutAttributeInfo,
	TArray<float>& OutData,
	int32 InTupleSize,
	HAPI_AttributeOwner InOwner)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("FHoudiniEngineUtils::HapiGetAttributeDataAsFloat"));

	OutAttributeInfo.exists = false;

	// Reset container size.
	OutData.SetNumUninitialized(0);

	int32 OriginalTupleSize = InTupleSize;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				InGeoId, InPartId, InAttribName,
				(HAPI_AttributeOwner)AttrIdx, &AttributeInfo), false);

			if (AttributeInfo.exists)
				break;
		}
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(), 
			InGeoId, InPartId, InAttribName,
			InOwner, &AttributeInfo), false);
	}

	if (!AttributeInfo.exists)
		return false;

	if (OriginalTupleSize > 0)
		AttributeInfo.tupleSize = OriginalTupleSize;

	// Allocate sufficient buffer for data.
	OutData.SetNum(AttributeInfo.count * AttributeInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(), 
		InGeoId, InPartId, InAttribName,
		&AttributeInfo, -1, &OutData[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	OutAttributeInfo = AttributeInfo;
	return true;
}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& OutAttributeInfo,
	TArray<int32>& OutData,
	const int32& InTupleSize,
	const HAPI_AttributeOwner& InOwner)
{
	OutAttributeInfo.exists = false;

	// Reset container size.
	OutData.SetNumUninitialized(0);

	int32 OriginalTupleSize = InTupleSize;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				InGeoId, InPartId, InAttribName,
				(HAPI_AttributeOwner)AttrIdx, &AttributeInfo), false);

			if (AttributeInfo.exists)
				break;
		}
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			InOwner, &AttributeInfo), false);
	}

	if (!AttributeInfo.exists)
		return false;

	if (OriginalTupleSize > 0)
		AttributeInfo.tupleSize = OriginalTupleSize;

	// Allocate sufficient buffer for data.
	OutData.SetNum(AttributeInfo.count * AttributeInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeIntData(
		FHoudiniEngine::Get().GetSession(),
		InGeoId, InPartId, InAttribName,
		&AttributeInfo, -1, &OutData[0], 0, AttributeInfo.count), false);

	// Store the retrieved attribute information.
	OutAttributeInfo = AttributeInfo;
	return true;
}


bool
FHoudiniEngineUtils::HapiGetAttributeDataAsString(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	const char * InAttribName,
	HAPI_AttributeInfo& OutAttributeInfo,
	TArray<FString>& OutData,
	int32 InTupleSize,
	HAPI_AttributeOwner InOwner)
{
	OutAttributeInfo.exists = false;

	// Reset container size.
	OutData.SetNumUninitialized(0);

	int32 OriginalTupleSize = InTupleSize;

	HAPI_AttributeInfo AttributeInfo;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 AttrIdx = 0; AttrIdx < HAPI_ATTROWNER_MAX; ++AttrIdx)
		{
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				InGeoId, InPartId, InAttribName,
				(HAPI_AttributeOwner)AttrIdx, &AttributeInfo), false);

			if (AttributeInfo.exists)
				break;
		}
	}
	else
	{
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InGeoId, InPartId, InAttribName,
			InOwner, &AttributeInfo), false);
	}

	if (!AttributeInfo.exists)
		return false;

	// Store the retrieved attribute information.
	OutAttributeInfo = AttributeInfo;

	if (OriginalTupleSize > 0)
		AttributeInfo.tupleSize = OriginalTupleSize;

	return FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(InGeoId, InPartId, AttributeInfo, InAttribName, OutData);

}

bool
FHoudiniEngineUtils::HapiGetAttributeDataAsStringFromInfo(
	const HAPI_NodeId& InGeoId,
	const HAPI_PartId& InPartId,
	HAPI_AttributeInfo& InAttributeInfo, 
	const char * InAttribName,
	TArray<FString>& OutData)
{
	if (!InAttributeInfo.exists)
		return false;

	// Extract the StringHandles
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.Init(-1, InAttributeInfo.count * InAttributeInfo.tupleSize);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeStringData(
		FHoudiniEngine::Get().GetSession(),
		InGeoId, InPartId, InAttribName, &InAttributeInfo,
		&StringHandles[0], 0, InAttributeInfo.count), false);

	// Set the output data size
	OutData.SetNum(InAttributeInfo.count);

	// Convert the StringHandles to FString.
	// We'll use a map to minimize the number of HAPI calls
	TMap<int32, FString> StringHandleToStringMap;
	for (int32 Idx = 0; Idx < StringHandles.Num(); ++Idx)
	{
		const HAPI_StringHandle& CurrentSH = StringHandles[Idx];
		if (CurrentSH < 0)
		{
			OutData[Idx] = TEXT("");
			continue;
		}

		FString* FoundString = StringHandleToStringMap.Find(CurrentSH);
		if (FoundString)
		{
			OutData[Idx] = *FoundString;
		}
		else
		{
			FString HapiString = TEXT("");
			FHoudiniEngineString::ToFString(CurrentSH, HapiString);

			StringHandleToStringMap.Add(CurrentSH, HapiString);
			OutData[Idx] = HapiString;
		}
	}

	return true;
}

bool
FHoudiniEngineUtils::HapiCheckAttributeExists(
	const HAPI_NodeId& GeoId, const HAPI_PartId& PartId,
	const char * AttribName, HAPI_AttributeOwner Owner)
{
	if (Owner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 OwnerIdx = 0; OwnerIdx < HAPI_ATTROWNER_MAX; OwnerIdx++)
		{
			if (HapiCheckAttributeExists(GeoId, PartId, AttribName, (HAPI_AttributeOwner)OwnerIdx))
			{
				return true;
			}
		}
	}
	else
	{
		HAPI_AttributeInfo AttribInfo;
		FHoudiniApi::AttributeInfo_Init(&AttribInfo);

		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, AttribName, Owner, &AttribInfo), false);

		return AttribInfo.exists;
	}

	return false;
}

bool
FHoudiniEngineUtils::IsAttributeInstancer(const HAPI_NodeId& GeoId, const HAPI_PartId& PartId, EHoudiniInstancerType& OutInstancerType)
{
	// Check for 
	// - HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE (unreal_instance) on points/detail
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, HAPI_ATTROWNER_POINT))
	{
		OutInstancerType = EHoudiniInstancerType::AttributeInstancer;
		return true;
	}

	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE_OVERRIDE, HAPI_ATTROWNER_DETAIL))
	{
		OutInstancerType = EHoudiniInstancerType::AttributeInstancer;
		return true;
	}

	// - HAPI_UNREAL_ATTRIB_INSTANCE (instance) on points
	if (FHoudiniEngineUtils::HapiCheckAttributeExists(GeoId, PartId, HAPI_UNREAL_ATTRIB_INSTANCE, HAPI_ATTROWNER_POINT))
	{
		OutInstancerType = EHoudiniInstancerType::OldSchoolAttributeInstancer;
		return true;
	}

	return false;
}

bool
FHoudiniEngineUtils::HapiGetParameterDataAsString(
	const HAPI_NodeId& NodeId, 
	const std::string& ParmName,
	const FString& DefaultValue,
	FString& OutValue)
{
	OutValue = DefaultValue;

	// Try to find the parameter by name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), false);
	
	if (ParmId < 0)
		return false;

	// Get the param info...
	HAPI_ParmInfo FoundParamInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParamInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParamInfo), false);

	// .. and value
	HAPI_StringHandle StringHandle;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmStringValues(
		FHoudiniEngine::Get().GetSession(), NodeId, false,
		&StringHandle, FoundParamInfo.stringValuesIndex, 1), false);

	// Convert the string handle to FString
	return FHoudiniEngineString::ToFString(StringHandle, OutValue);
}

bool 
FHoudiniEngineUtils::HapiGetParameterDataAsInteger(
	const HAPI_NodeId& NodeId,
	const std::string& ParmName,
	const int32& DefaultValue,
	int32& OutValue)
{
	OutValue = DefaultValue;	

	// Try to find the parameter by its name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), false);
		
	if (ParmId < 0)
		return false;

	// Get the param info...
	HAPI_ParmInfo FoundParmInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParmInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParmInfo), false);
	
	// .. and value
	int32 Value = DefaultValue;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIntValues(
		FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		FoundParmInfo.intValuesIndex, 1), false);

	OutValue = Value;

	return true;
}


bool
FHoudiniEngineUtils::HapiGetParameterDataAsFloat(
	const HAPI_NodeId& NodeId,
	const std::string& ParmName,
	const float& DefaultValue,
	float& OutValue)
{
	OutValue = DefaultValue;

	// Try to find the parameter by its name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), false);

	if (ParmId < 0)
		return false;

	// Get the param info...
	HAPI_ParmInfo FoundParmInfo;
	FHoudiniApi::ParmInfo_Init(&FoundParmInfo);
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParmInfo), false);

	// .. and value
	float Value = DefaultValue;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmFloatValues(
		FHoudiniEngine::Get().GetSession(), NodeId, &Value,
		FoundParmInfo.floatValuesIndex, 1), false);

	OutValue = Value;

	return true;
}

HAPI_ParmId FHoudiniEngineUtils::HapiFindParameterByNameOrTag(const HAPI_NodeId& NodeId, const std::string& ParmName, HAPI_ParmInfo& FoundParmInfo) 
{
	FHoudiniApi::ParmInfo_Init(&FoundParmInfo);

	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);
	FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), NodeId, &NodeInfo);
	if (NodeInfo.parmCount <= 0)
		return -1;

	HAPI_ParmId ParmId = HapiFindParameterByNameOrTag(NodeInfo.id, ParmName);
	if ((ParmId < 0) || (ParmId >= NodeInfo.parmCount))
		return -1;

	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmInfo(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmId, &FoundParmInfo), -1);

	return ParmId;
}


HAPI_ParmId FHoudiniEngineUtils::HapiFindParameterByNameOrTag(const HAPI_NodeId& NodeId, const std::string& ParmName)
{
	// First, try to find the parameter by its name
	HAPI_ParmId ParmId = -1;
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmIdFromName(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), -1);

	if (ParmId >= 0)
		return ParmId;

	// Second, try to find it by its tag
	HOUDINI_CHECK_ERROR_RETURN( FHoudiniApi::GetParmWithTag(
		FHoudiniEngine::Get().GetSession(),
		NodeId, ParmName.c_str(), &ParmId), -1);

	if (ParmId >= 0)
		return ParmId;

	return -1;
}

int32
FHoudiniEngineUtils::HapiGetAttributeOfType(
	const HAPI_NodeId& GeoId,
	const HAPI_NodeId& PartId,
	const HAPI_AttributeOwner& AttributeOwner,
	const HAPI_AttributeTypeInfo& AttributeType,
	TArray< HAPI_AttributeInfo >& MatchingAttributesInfo,
	TArray< FString >& MatchingAttributesName)
{
	int32 NumberOfAttributeFound = 0;

	// Get the part infos
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId, &PartInfo), NumberOfAttributeFound);

	// Get All attribute names for that part
	int32 nAttribCount = PartInfo.attributeCounts[AttributeOwner];

	TArray<HAPI_StringHandle> AttribNameSHArray;
	AttribNameSHArray.SetNum(nAttribCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAttributeNames(
		FHoudiniEngine::Get().GetSession(),
		GeoId, PartId, AttributeOwner,
		AttribNameSHArray.GetData(), nAttribCount), NumberOfAttributeFound);

	// Iterate on all the attributes, and get their part infos to get their type    
	for (int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx)
	{
		// Get the name ...
		FString HapiString = TEXT("");
		FHoudiniEngineString::ToFString(AttribNameSHArray[Idx], HapiString);

		// ... then the attribute info
		HAPI_AttributeInfo AttrInfo;
		FHoudiniApi::AttributeInfo_Init(&AttrInfo);

		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			GeoId, PartId, TCHAR_TO_UTF8(*HapiString),
			AttributeOwner, &AttrInfo))
			continue;

		if (!AttrInfo.exists)
			continue;

		// ... check the type
		if (AttrInfo.typeInfo != AttributeType)
			continue;

		MatchingAttributesInfo.Add(AttrInfo);
		MatchingAttributesName.Add(HapiString);

		NumberOfAttributeFound++;
	}

	return NumberOfAttributeFound;
}

HAPI_PartInfo
FHoudiniEngineUtils::ToHAPIPartInfo(const FHoudiniPartInfo& InHPartInfo)
{
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);

	PartInfo.id = InHPartInfo.PartId;
	//PartInfo.nameSH = InHPartInfo.Name;

	switch (InHPartInfo.Type)
	{
		case EHoudiniPartType::Mesh:
			PartInfo.type = HAPI_PARTTYPE_MESH;
			break;
		case EHoudiniPartType::Curve:
			PartInfo.type = HAPI_PARTTYPE_CURVE;
			break;
		case EHoudiniPartType::Instancer:
			PartInfo.type = HAPI_PARTTYPE_INSTANCER;
			break;
		case EHoudiniPartType::Volume:
			PartInfo.type = HAPI_PARTTYPE_VOLUME;
			break;
		default:
		case EHoudiniPartType::Invalid:
			PartInfo.type = HAPI_PARTTYPE_INVALID;
			break;
	}

	PartInfo.faceCount = InHPartInfo.FaceCount;
	PartInfo.vertexCount = InHPartInfo.VertexCount;
	PartInfo.pointCount = InHPartInfo.PointCount;

	PartInfo.attributeCounts[HAPI_ATTROWNER_POINT] = InHPartInfo.PointAttributeCounts;
	PartInfo.attributeCounts[HAPI_ATTROWNER_VERTEX] = InHPartInfo.VertexAttributeCounts;
	PartInfo.attributeCounts[HAPI_ATTROWNER_PRIM] = InHPartInfo.PrimitiveAttributeCounts;
	PartInfo.attributeCounts[HAPI_ATTROWNER_DETAIL] = InHPartInfo.DetailAttributeCounts;

	PartInfo.isInstanced = InHPartInfo.bIsInstanced;

	PartInfo.instancedPartCount = InHPartInfo.InstancedPartCount;
	PartInfo.instanceCount = InHPartInfo.InstanceCount;

	PartInfo.hasChanged = InHPartInfo.bHasChanged;

	return PartInfo;
}

int32
FHoudiniEngineUtils::AddMeshSocketsToArray_DetailAttribute(
	const HAPI_NodeId& GeoId,
	const HAPI_PartId& PartId,
	TArray< FHoudiniMeshSocket >& AllSockets,
	const bool& isPackedPrim)
{
	int32 FoundSocketCount = 0;

	// Attributes we are interested in.
	// Position
	TArray<float> Positions;
	HAPI_AttributeInfo AttribInfoPositions;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

	// Rotation
	bool bHasRotation = false;
	TArray<float> Rotations;
	HAPI_AttributeInfo AttribInfoRotations;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

	// Scale
	bool bHasScale = false;
	TArray<float> Scales;
	HAPI_AttributeInfo AttribInfoScales;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

	// Socket Name
	bool bHasNames = false;
	TArray<FString> Names;
	HAPI_AttributeInfo AttribInfoNames;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

	// Socket Actor
	bool bHasActors = false;
	TArray<FString> Actors;
	HAPI_AttributeInfo AttribInfoActors;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

	// Socket Tags
	bool bHasTags = false;
	TArray<FString> Tags;
	HAPI_AttributeInfo AttribInfoTags;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);

	// Lambda function for creating the socket and adding it to the array
	// Shared between the by Attribute / by Group methods	
	auto AddSocketToArray = [&](const int32& PointIdx)
	{
		FHoudiniMeshSocket CurrentSocket;
		FVector currentPosition = FVector::ZeroVector;
		if (Positions.IsValidIndex(PointIdx * 3 + 2))
		{
			currentPosition.X = Positions[PointIdx * 3] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Y = Positions[PointIdx * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Z = Positions[PointIdx * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		FVector currentScale = FVector::OneVector;
		if (bHasScale && Scales.IsValidIndex(PointIdx * 3 + 2))
		{
			currentScale.X = Scales[PointIdx * 3];
			currentScale.Y = Scales[PointIdx * 3 + 2];
			currentScale.Z = Scales[PointIdx * 3 + 1];
		}

		FQuat currentRotation = FQuat::Identity;
		if (bHasRotation && Rotations.IsValidIndex(PointIdx * 4 + 3))
		{
			currentRotation.X = Rotations[PointIdx * 4];
			currentRotation.Y = Rotations[PointIdx * 4 + 2];
			currentRotation.Z = Rotations[PointIdx * 4 + 1];
			currentRotation.W = -Rotations[PointIdx * 4 + 3];
		}

		if (bHasNames && Names.IsValidIndex(PointIdx))
			CurrentSocket.Name = Names[PointIdx];

		if (bHasActors && Actors.IsValidIndex(PointIdx))
			CurrentSocket.Actor = Actors[PointIdx];

		if (bHasTags && Tags.IsValidIndex(PointIdx))
			CurrentSocket.Tag = Tags[PointIdx];

		// If the scale attribute wasn't set on all socket, we might end up
		// with a zero scale socket, avoid that.
		if (currentScale == FVector::ZeroVector)
			currentScale = FVector::OneVector;

		CurrentSocket.Transform.SetLocation(currentPosition);
		CurrentSocket.Transform.SetRotation(currentRotation);
		CurrentSocket.Transform.SetScale3D(currentScale);

		// We want to make sure we're not adding the same socket multiple times
		AllSockets.AddUnique(CurrentSocket);

		FoundSocketCount++;

		return true;
	};


	// Lambda function for reseting the arrays/attributes
	auto ResetArraysAndAttr = [&]()
	{
		// Position
		Positions.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

		// Rotation
		bHasRotation = false;
		Rotations.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

		// Scale
		bHasScale = false;
		Scales.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

		// Socket Name
		bHasNames = false;
		Names.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

		// Socket Actor
		bHasActors = false;
		Actors.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

		// Socket Tags
		bHasTags = false;
		Tags.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);
	};

	//-------------------------------------------------------------------------
	// FIND SOCKETS BY DETAIL ATTRIBUTES
	//-------------------------------------------------------------------------

	int32 SocketIdx = 0;
	bool HasSocketAttributes = true;	
	while (HasSocketAttributes)
	{
		// Build the current socket's prefix
		FString SocketAttrPrefix = TEXT(HAPI_UNREAL_ATTRIB_MESH_SOCKET_PREFIX) + FString::FromInt(SocketIdx);

		// Reset the arrays and attributes
		ResetArraysAndAttr();

		// Retrieve position data.
		FString SocketPosAttr = SocketAttrPrefix + TEXT("_pos");
		if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId, PartId, TCHAR_TO_ANSI(*SocketPosAttr),
			AttribInfoPositions, Positions, 0, HAPI_ATTROWNER_DETAIL))
			break;

		if (!AttribInfoPositions.exists)
		{
			// No need to keep looking for socket attributes
			HasSocketAttributes = false;
			break;
		}

		// Retrieve rotation data.
		FString SocketRotAttr = SocketAttrPrefix + TEXT("_rot");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketRotAttr), AttribInfoRotations, Rotations, 0, HAPI_ATTROWNER_DETAIL))
			bHasRotation = true;

		// Retrieve scale data.
		FString SocketScaleAttr = SocketAttrPrefix + TEXT("_scale");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketScaleAttr), AttribInfoScales, Scales, 0, HAPI_ATTROWNER_DETAIL))
			bHasScale = true;

		// Retrieve mesh socket names.
		FString SocketNameAttr = SocketAttrPrefix + TEXT("_name");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketNameAttr), AttribInfoNames, Names))
			bHasNames = true;

		// Retrieve mesh socket actor.
		FString SocketActorAttr = SocketAttrPrefix + TEXT("_actor");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketActorAttr), AttribInfoActors, Actors))
			bHasActors = true;

		// Retrieve mesh socket tags.
		FString SocketTagAttr = SocketAttrPrefix + TEXT("_tag");
		if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
			GeoId, PartId,
			TCHAR_TO_ANSI(*SocketTagAttr), AttribInfoTags, Tags))
			bHasTags = true;

		// Add the socket to the array
		AddSocketToArray(0);

		// Try to find the next socket
		SocketIdx++;
	}

	return FoundSocketCount;
}


int32
FHoudiniEngineUtils::AddMeshSocketsToArray_Group(
	const HAPI_NodeId& GeoId,
	const HAPI_PartId& PartId,
	TArray<FHoudiniMeshSocket>& AllSockets,
	const bool& isPackedPrim)
{
	// Attributes we are interested in.
	// Position
	TArray<float> Positions;
	HAPI_AttributeInfo AttribInfoPositions;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

	// Rotation
	bool bHasRotation = false;
	TArray<float> Rotations;
	HAPI_AttributeInfo AttribInfoRotations;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

	// Scale
	bool bHasScale = false;
	TArray<float> Scales;
	HAPI_AttributeInfo AttribInfoScales;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

	// We can also get the sockets rotation from the normal
	bool bHasNormals = false;
	TArray<float> Normals;
	HAPI_AttributeInfo AttribInfoNormals;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNormals);

	// Socket Name
	bool bHasNames = false;
	TArray<FString> Names;
	HAPI_AttributeInfo AttribInfoNames;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

	// Socket Actor
	bool bHasActors = false;
	TArray<FString> Actors;
	HAPI_AttributeInfo AttribInfoActors;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

	// Socket Tags
	bool bHasTags = false;
	TArray<FString> Tags;
	HAPI_AttributeInfo AttribInfoTags;
	FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);

	// Lambda function for creating the socket and adding it to the array
	// Shared between the by Attribute / by Group methods
	int32 FoundSocketCount = 0;
	auto AddSocketToArray = [&](const int32& PointIdx)
	{
		FHoudiniMeshSocket CurrentSocket;
		FVector currentPosition = FVector::ZeroVector;
		if (Positions.IsValidIndex(PointIdx * 3 + 2))
		{
			currentPosition.X = Positions[PointIdx * 3] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Y = Positions[PointIdx * 3 + 2] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
			currentPosition.Z = Positions[PointIdx * 3 + 1] * HAPI_UNREAL_SCALE_FACTOR_POSITION;
		}

		FVector currentScale = FVector::OneVector;
		if (bHasScale && Scales.IsValidIndex(PointIdx * 3 + 2))
		{
			currentScale.X = Scales[PointIdx * 3];
			currentScale.Y = Scales[PointIdx * 3 + 2];
			currentScale.Z = Scales[PointIdx * 3 + 1];
		}

		FQuat currentRotation = FQuat::Identity;
		if (bHasRotation && Rotations.IsValidIndex(PointIdx * 4 + 3))
		{
			currentRotation.X = Rotations[PointIdx * 4];
			currentRotation.Y = Rotations[PointIdx * 4 + 2];
			currentRotation.Z = Rotations[PointIdx * 4 + 1];
			currentRotation.W = -Rotations[PointIdx * 4 + 3];
		}
		else if (bHasNormals && Normals.IsValidIndex(PointIdx * 3 + 2))
		{
			FVector vNormal;
			vNormal.X = Normals[PointIdx * 3];
			vNormal.Y = Normals[PointIdx * 3 + 2];
			vNormal.Z = Normals[PointIdx * 3 + 1];

			if (vNormal != FVector::ZeroVector)
				currentRotation = FQuat::FindBetween(FVector::UpVector, vNormal);
		}

		if (bHasNames && Names.IsValidIndex(PointIdx))
			CurrentSocket.Name = Names[PointIdx];

		if (bHasActors && Actors.IsValidIndex(PointIdx))
			CurrentSocket.Actor = Actors[PointIdx];

		if (bHasTags && Tags.IsValidIndex(PointIdx))
			CurrentSocket.Tag = Tags[PointIdx];

		// If the scale attribute wasn't set on all socket, we might end up
		// with a zero scale socket, avoid that.
		if (currentScale == FVector::ZeroVector)
			currentScale = FVector::OneVector;

		CurrentSocket.Transform.SetLocation(currentPosition);
		CurrentSocket.Transform.SetRotation(currentRotation);
		CurrentSocket.Transform.SetScale3D(currentScale);

		// We want to make sure we're not adding the same socket multiple times
		AllSockets.AddUnique(CurrentSocket);

		FoundSocketCount++;

		return true;
	};


	// Lambda function for reseting the arrays/attributes
	auto ResetArraysAndAttr = [&]()
	{
		// Position
		Positions.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoPositions);

		// Rotation
		bHasRotation = false;
		Rotations.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoRotations);

		// Scale
		bHasScale = false;
		Scales.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoScales);

		// When using socket groups, we can also get the sockets rotation from the normal
		bHasNormals = false;
		Normals.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoNormals);

		// Socket Name
		bHasNames = false;
		Names.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoNames);

		// Socket Actor
		bHasActors = false;
		Actors.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoActors);

		// Socket Tags
		bHasTags = false;
		Tags.Empty();
		FHoudiniApi::AttributeInfo_Init(&AttribInfoTags);
	};

	//-------------------------------------------------------------------------
	// FIND SOCKETS BY POINT GROUPS
	//-------------------------------------------------------------------------

	// Get object / geo group memberships for primitives.
	TArray<FString> GroupNames;
	if (!FHoudiniEngineUtils::HapiGetGroupNames(
		GeoId, PartId, HAPI_GROUPTYPE_POINT, isPackedPrim, GroupNames))
	{
		HOUDINI_LOG_MESSAGE(TEXT("GetMeshSocketList: Geo [%d] Part [%d] non-fatal error reading point group names"), GeoId, PartId);
	}

	// First, we want to make sure we have at least one socket group before continuing
	bool bHasSocketGroup = false;
	for (int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx)
	{
		const FString & GroupName = GroupNames[GeoGroupNameIdx];
		if (GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX, ESearchCase::IgnoreCase)
			|| GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX_OLD, ESearchCase::IgnoreCase))
		{
			bHasSocketGroup = true;
			break;
		}
	}

	if (!bHasSocketGroup)
		return FoundSocketCount;

	// Get the part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), GeoId, PartId, &PartInfo))
		return false;

	// Reset the data arrays and attributes
	ResetArraysAndAttr();	

	// Retrieve position data.
	if (!FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_POSITION, AttribInfoPositions, Positions))
		return false;

	// Retrieve rotation data.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_ROTATION, AttribInfoRotations, Rotations))
		bHasRotation = true;

	// Retrieve normal data.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_NORMAL, AttribInfoNormals, Normals))
		bHasNormals = true;

	// Retrieve scale data.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_SCALE, AttribInfoScales, Scales))
		bHasScale = true;

	// Retrieve mesh socket names.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME, AttribInfoNames, Names))
		bHasNames = true;
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_NAME_OLD, AttribInfoNames, Names))
		bHasNames = true;

	// Retrieve mesh socket actor.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR, AttribInfoActors, Actors))
		bHasActors = true;
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_ACTOR_OLD, AttribInfoActors, Actors))
		bHasActors = true;

	// Retrieve mesh socket tags.
	if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG, AttribInfoTags, Tags))
		bHasTags = true;
	else if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
		GeoId, PartId, HAPI_UNREAL_ATTRIB_MESH_SOCKET_TAG_OLD, AttribInfoTags, Tags))
		bHasTags = true;

	// Extracting Sockets vertices
	for (int32 GeoGroupNameIdx = 0; GeoGroupNameIdx < GroupNames.Num(); ++GeoGroupNameIdx)
	{
		const FString & GroupName = GroupNames[GeoGroupNameIdx];
		if (!GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX, ESearchCase::IgnoreCase)
			&& !GroupName.StartsWith(HAPI_UNREAL_GROUP_SOCKET_PREFIX_OLD, ESearchCase::IgnoreCase))
			continue;

		bool AllEquals = false;
		TArray< int32 > PointGroupMembership;
		FHoudiniEngineUtils::HapiGetGroupMembership(
			GeoId, PartInfo, HAPI_GROUPTYPE_POINT, GroupName, PointGroupMembership, AllEquals);

		// Go through all primitives.
		for (int32 PointIdx = 0; PointIdx < PointGroupMembership.Num(); ++PointIdx)
		{
			if (PointGroupMembership[PointIdx] == 0)
			{
				if (AllEquals)
					break;
				else
					continue;
			}

			// Add the corresponding socket to the array
			AddSocketToArray(PointIdx);
		}
	}

	return FoundSocketCount;
}

bool
FHoudiniEngineUtils::AddMeshSocketsToStaticMesh(
	UStaticMesh* StaticMesh,
	TArray<FHoudiniMeshSocket >& AllSockets,
	const bool& CleanImportSockets)
{
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return false;

	// Remove the sockets from the previous cook!
	if (CleanImportSockets)
	{
		StaticMesh->Sockets.RemoveAll([=](UStaticMeshSocket* Socket) { return Socket ? Socket->bSocketCreatedAtImport : true; });
	}

	if (AllSockets.Num() <= 0)
		return true;

	for (int32 nSocket = 0; nSocket < AllSockets.Num(); nSocket++)
	{
		// Create a new Socket
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(StaticMesh);
		if (!Socket || Socket->IsPendingKill())
			continue;

		Socket->RelativeLocation = AllSockets[nSocket].Transform.GetLocation();
		Socket->RelativeRotation = FRotator(AllSockets[nSocket].Transform.GetRotation());
		Socket->RelativeScale = AllSockets[nSocket].Transform.GetScale3D();

		if (!AllSockets[nSocket].Name.IsEmpty())
		{
			Socket->SocketName = FName(*AllSockets[nSocket].Name);
		}
		else
		{
			// Having sockets with empty names can lead to various issues, so we'll create one now
			FString SocketName = TEXT("Socket ") + FString::FromInt(nSocket);
			Socket->SocketName = FName(*SocketName);
		}

		// Socket Tag
		FString Tag;
		if (!AllSockets[nSocket].Tag.IsEmpty())
			Tag = AllSockets[nSocket].Tag;

		// The actor will be stored temporarily in the socket's Tag as we need a StaticMeshComponent to add an actor to the socket
		if (!AllSockets[nSocket].Actor.IsEmpty())
			Tag += TEXT("|") + AllSockets[nSocket].Actor;

		Socket->Tag = Tag;
		Socket->bSocketCreatedAtImport = true;

		StaticMesh->Sockets.Add(Socket);
	}

	return true;
}

bool
FHoudiniEngineUtils::CreateAttributesFromTags(
	const HAPI_NodeId& NodeId, 
	const HAPI_PartId& PartId,
	const TArray<FName>& Tags )
{
	if (Tags.Num() <= 0)
		return false;

	HAPI_Result Result = HAPI_RESULT_FAILURE;

	// Get the destination part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), NodeId, PartId, &PartInfo), false);

	bool NeedToCommitGeo = false;
	for (int32 TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
	{
		FString TagString;
		Tags[TagIdx].ToString(TagString);
		SanitizeHAPIVariableName(TagString);
		
		// Create a primitive attribute for the tag
		HAPI_AttributeInfo AttributeInfo;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfo);

		AttributeInfo.count = PartInfo.faceCount;
		AttributeInfo.tupleSize = 1;
		AttributeInfo.exists = true;
		AttributeInfo.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfo.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfo.originalOwner = HAPI_ATTROWNER_INVALID;
		AttributeInfo.typeInfo = HAPI_ATTRIBUTE_TYPE_NONE;

		FString AttributeName = TEXT(HAPI_UNREAL_ATTRIB_TAG_PRE) + FString::FromInt(TagIdx);
		AttributeName.RemoveSpacesInline();

		Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo);

		if (Result != HAPI_RESULT_SUCCESS)
			continue;

		TArray<const char*> TagStr;
		TagStr.Add(FHoudiniEngineUtils::ExtractRawString(TagString));

		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, TCHAR_TO_ANSI(*AttributeName), &AttributeInfo,
			TagStr.GetData(), 0, AttributeInfo.count);

		if (HAPI_RESULT_SUCCESS == Result)
			NeedToCommitGeo = true;

		// Free memory for allocated by ExtractRawString
		FHoudiniEngineUtils::FreeRawStringMemory(TagStr);
	}

	return NeedToCommitGeo;
}

bool
FHoudiniEngineUtils::CreateGroupsFromTags(
	const HAPI_NodeId& NodeId,
	const HAPI_PartId& PartId, 
	const TArray<FName>& Tags )
{
	if (Tags.Num() <= 0)
		return true;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	
	// Get the destination part info
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(	FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(),	NodeId, PartId, &PartInfo), false);

	bool NeedToCommitGeo = false;
	for (int32 TagIdx = 0; TagIdx < Tags.Num(); TagIdx++)
	{
		FString TagString;
		Tags[TagIdx].ToString(TagString);
		SanitizeHAPIVariableName(TagString);
		
		const char * TagStr = FHoudiniEngineUtils::ExtractRawString(TagString);

		// Create a primitive group for this tag
		if ( HAPI_RESULT_SUCCESS == FHoudiniApi::AddGroup(
			FHoudiniEngine::Get().GetSession(),
			NodeId, 0, HAPI_GROUPTYPE_PRIM, TagStr) )
		{
			// Set the group's Memberships
			TArray<int> GroupArray;
			GroupArray.Init(1, PartInfo.faceCount);

			if ( HAPI_RESULT_SUCCESS == FHoudiniApi::SetGroupMembership(
				FHoudiniEngine::Get().GetSession(),
				NodeId, PartId, HAPI_GROUPTYPE_PRIM, TagStr,
				GroupArray.GetData(), 0, PartInfo.faceCount) )
			{
				NeedToCommitGeo = true;
			}
		}

		// Free memory allocated by ExtractRawString()
		FHoudiniEngineUtils::FreeRawStringMemory(TagStr);
	}

	return NeedToCommitGeo;
}


bool
FHoudiniEngineUtils::SanitizeHAPIVariableName(FString& String)
{
	// Only keep alphanumeric characters, underscores
	// Also, if the first character is a digit, append an underscore at the beginning
	TArray<TCHAR>& StrArray = String.GetCharArray();
	if (StrArray.Num() <= 0)
		return false;

	for (auto& CurChar : StrArray)
	{
		const bool bIsValid = (CurChar >= TEXT('A') && CurChar <= TEXT('Z'))
			|| (CurChar >= TEXT('a') && CurChar <= TEXT('z'))
			|| (CurChar >= TEXT('0') && CurChar <= TEXT('9'))
			|| (CurChar == TEXT('_')) || (CurChar == TEXT('\0'));

		if(bIsValid)
			continue;

		CurChar = TEXT('_');
	}

	if (StrArray.Num() > 0)
	{
		TCHAR FirstChar = StrArray[0];
		if (FirstChar >= TEXT('0') && FirstChar <= TEXT('9'))
			StrArray.Insert(TEXT('_'), 0);
	}

	return true;
}

bool
FHoudiniEngineUtils::GetUnrealTagAttributes(
	const HAPI_NodeId& GeoId, const HAPI_PartId& PartId, TArray<FName>& OutTags)
{
	FString TagAttribBase = TEXT("unreal_tag_");
	bool bAttributeFound = true;
	int32 TagIdx = 0;
	while (bAttributeFound)
	{
		FString CurrentTagAttr = TagAttribBase + FString::FromInt(TagIdx++);
		bAttributeFound = HapiCheckAttributeExists(GeoId, PartId, TCHAR_TO_UTF8(*CurrentTagAttr), HAPI_ATTROWNER_PRIM);
		if (!bAttributeFound)
			break;

		// found the unreal_tag_X attribute, get its value and add it to the array
		FString TagValue = FString();

		// Create an AttributeInfo
		{
			HAPI_AttributeInfo AttributeInfo;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfo);
			TArray<FString> StringData;
			if (FHoudiniEngineUtils::HapiGetAttributeDataAsString(
				GeoId, PartId, TCHAR_TO_UTF8(*CurrentTagAttr), AttributeInfo, StringData, 1, HAPI_ATTROWNER_PRIM))
			{
				TagValue = StringData[0];
			}
		}

		FName NameTag = *TagValue;
		OutTags.Add(NameTag);
	}

	return true;
}


int32
FHoudiniEngineUtils::GetPropertyAttributeList(
	const FHoudiniGeoPartObject& InHGPO, TArray<FHoudiniGenericAttribute>& OutFoundPropertyAttributes)
{
	// Get all the detail uprop attributes on the HGPO
	int32 FoundCount = FHoudiniEngineUtils::GetGenericAttributeList(
		(HAPI_NodeId)InHGPO.GeoInfo.NodeId, (HAPI_PartId)InHGPO.PartInfo.PartId,
		HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutFoundPropertyAttributes, HAPI_ATTROWNER_DETAIL);

	// .. then the primitive uprop attributes
	FoundCount += FHoudiniEngineUtils::GetGenericAttributeList(
		(HAPI_NodeId)InHGPO.GeoInfo.NodeId, (HAPI_PartId)InHGPO.PartInfo.PartId,
		HAPI_UNREAL_ATTRIB_GENERIC_UPROP_PREFIX, OutFoundPropertyAttributes, HAPI_ATTROWNER_PRIM);

	return FoundCount;
}


int32
FHoudiniEngineUtils::GetGenericAttributeList(
	const HAPI_NodeId& InGeoNodeId,
	const HAPI_PartId& InPartId,
	const FString& InGenericAttributePrefix,
	TArray<FHoudiniGenericAttribute>& OutFoundAttributes,
	const HAPI_AttributeOwner& AttributeOwner,
	const int32& InAttribIndex)
{
	// Get the part info to get the attribute counts for the specified owner
	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(), InGeoNodeId, InPartId, &PartInfo), false);
	
	int32 nAttribCount = PartInfo.attributeCounts[AttributeOwner];

	// Get all attribute names for that part
	TArray<HAPI_StringHandle> AttribNameSHArray;
	AttribNameSHArray.SetNum(nAttribCount);
	if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeNames(
		FHoudiniEngine::Get().GetSession(),
		InGeoNodeId, InPartId, AttributeOwner,
		AttribNameSHArray.GetData(), nAttribCount))
	{
		return 0;
	}	

	// For everything but detail attribute,
	// if an attribute index was specified, only extract the attribute value for that specific index
	// if not, extract all values for the given attribute
	bool HandleSplit = false;
	int32 AttribIndex = -1;
	if ((AttributeOwner != HAPI_ATTROWNER_DETAIL) && (InAttribIndex != -1))
	{
		// The index has already been specified so we'll use it
		HandleSplit = true;
		AttribIndex = InAttribIndex;
	}

	int32 FoundCount = 0;
	for (int32 Idx = 0; Idx < AttribNameSHArray.Num(); ++Idx)
	{
		int32 AttribNameSH = (int32)AttribNameSHArray[Idx];
		FString AttribName = TEXT("");
		FHoudiniEngineString::ToFString(AttribNameSH, AttribName);
		if (!AttribName.StartsWith(InGenericAttributePrefix, ESearchCase::IgnoreCase))
			continue;

		// Get the Attribute Info
		HAPI_AttributeInfo AttribInfo;
		FHoudiniApi::AttributeInfo_Init(&AttribInfo);
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			InGeoNodeId, InPartId,
			TCHAR_TO_UTF8(*AttribName), AttributeOwner, &AttribInfo))
		{
			// failed to get that attribute's info
			continue;
		}

		int32 AttribStart = 0;
		int32 AttribCount = AttribInfo.count;
		if (HandleSplit)
		{
			// For split primitives, we need to only get only one value for the proper split prim
			// Make sure that the split index is valid
			if (AttribIndex >= 0 && AttribIndex < AttribInfo.count)
			{
				AttribStart = AttribIndex;
				AttribCount = 1;
			}
		}
		
		//
		FHoudiniGenericAttribute CurrentGenericAttribute;
		// Remove the generic attribute prefix
		CurrentGenericAttribute.AttributeName = AttribName.Right(AttribName.Len() - InGenericAttributePrefix.Len());

		CurrentGenericAttribute.AttributeOwner = (EAttribOwner)AttribInfo.owner;

		// Get the attribute type and tuple size
		CurrentGenericAttribute.AttributeType = (EAttribStorageType)AttribInfo.storage;
		CurrentGenericAttribute.AttributeCount = AttribInfo.count;
		CurrentGenericAttribute.AttributeTupleSize = AttribInfo.tupleSize;

		if (CurrentGenericAttribute.AttributeType == EAttribStorageType::FLOAT64)
		{
			// Initialize the value array
			CurrentGenericAttribute.DoubleValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeFloat64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				TCHAR_TO_UTF8(*AttribName), &AttribInfo, 0,
				CurrentGenericAttribute.DoubleValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}
		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::FLOAT)
		{
			// Initialize the value array
			TArray<float> FloatValues;
			FloatValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeFloatData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				0, FloatValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}

			// Convert them to double
			CurrentGenericAttribute.DoubleValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);
			for (int32 n = 0; n < FloatValues.Num(); n++)
				CurrentGenericAttribute.DoubleValues[n] = (double)FloatValues[n];

		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::INT64)
		{
			// Initialize the value array
			CurrentGenericAttribute.IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeInt64Data(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				0, CurrentGenericAttribute.IntValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}
		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::INT)
		{
			// Initialize the value array
			TArray<int32> IntValues;
			IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the value(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeIntData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				0, IntValues.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}

			// Convert them to int64
			CurrentGenericAttribute.IntValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);
			for (int32 n = 0; n < IntValues.Num(); n++)
				CurrentGenericAttribute.IntValues[n] = (int64)IntValues[n];

		}
		else if (CurrentGenericAttribute.AttributeType == EAttribStorageType::STRING)
		{
			// Initialize a string handle array
			TArray<HAPI_StringHandle> HapiSHArray;
			HapiSHArray.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			// Get the string handle(s)
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetAttributeStringData(
				FHoudiniEngine::Get().GetSession(),
				InGeoNodeId, InPartId,
				TCHAR_TO_UTF8(*AttribName), &AttribInfo,
				HapiSHArray.GetData(),
				AttribStart, AttribCount))
			{
				// failed to get that attribute's data
				continue;
			}

			// Convert them to FString
			CurrentGenericAttribute.StringValues.SetNumZeroed(AttribCount * AttribInfo.tupleSize);

			for (int32 IdxSH = 0; IdxSH < HapiSHArray.Num(); IdxSH++)
			{
				FString CurrentString;
				FHoudiniEngineString::ToFString(HapiSHArray[IdxSH], CurrentString);
				CurrentGenericAttribute.StringValues[IdxSH] = CurrentString;
			}
		}
		else
		{
			// Unsupported type, skipping!
			continue;
		}

		// We can add the UPropertyAttribute to the array
		OutFoundAttributes.Add(CurrentGenericAttribute);
		FoundCount++;
	}

	return FoundCount;
}


void
FHoudiniEngineUtils::UpdateAllPropertyAttributesOnObject(
	UObject* InObject, const FHoudiniGeoPartObject& InHGPO)
{
	if (!InObject || InObject->IsPendingKill())
		return;

	// Get the list of all the Properties to modify from the HGPO's attributes
	TArray<FHoudiniGenericAttribute> PropertiesAttributesToModify;
	if (!FHoudiniEngineUtils::GetPropertyAttributeList(InHGPO, PropertiesAttributesToModify))
		return;

	// Iterate over the found Property attributes
	for (auto CurrentPropAttribute : PropertiesAttributesToModify)
	{
		// Get the current Property Attribute
		const FString& CurrentPropertyName = CurrentPropAttribute.AttributeName;
		if (CurrentPropertyName.IsEmpty())
			continue;

		if (!FHoudiniGenericAttribute::UpdatePropertyAttributeOnObject(InObject, CurrentPropAttribute))
			continue;

		// Success!
		FString ClassName = InObject->GetClass() ? InObject->GetClass()->GetName() : TEXT("Object");
		FString ObjectName = InObject->GetName();
		HOUDINI_LOG_MESSAGE(TEXT("Modified UProperty %s on %s named %s"), *CurrentPropertyName, *ClassName, *ObjectName);
	}
}

void
FHoudiniEngineUtils::AddHoudiniMetaInformationToPackage(
	UPackage * Package, UObject * Object, const FString& Key, const FString& Value)
{
	if (!Package || Package->IsPendingKill())
		return;

	UMetaData * MetaData = Package->GetMetaData();
	if (MetaData && !MetaData->IsPendingKill())
		MetaData->SetValue(Object, *Key, *Value);
}

bool
FHoudiniEngineUtils::ContainsInvalidLightmapFaces(const FRawMesh & RawMesh, int32 LightmapSourceIdx)
{
	const TArray< FVector2D > & LightmapUVs = RawMesh.WedgeTexCoords[LightmapSourceIdx];
	const TArray< uint32 > & Indices = RawMesh.WedgeIndices;

	if (LightmapUVs.Num() != Indices.Num())
	{
		// This is invalid raw mesh; by design we consider that it contains invalid lightmap faces.
		return true;
	}

	for (int32 Idx = 0; Idx < Indices.Num(); Idx += 3)
	{
		const FVector2D & uv0 = LightmapUVs[Idx + 0];
		const FVector2D & uv1 = LightmapUVs[Idx + 1];
		const FVector2D & uv2 = LightmapUVs[Idx + 2];

		if (uv0 == uv1 && uv1 == uv2)
		{
			// Detect invalid lightmap face, can stop.
			return true;
		}
	}

	// Otherwise there are no invalid lightmap faces.
	return false;
}

void
FHoudiniEngineUtils::CreateSlateNotification(
	const FString& NotificationString, const float& NotificationExpire, const float& NotificationFadeOut )
{
#if WITH_EDITOR
	// Check whether we want to display Slate notifications.
	bool bDisplaySlateCookingNotifications = true;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault<UHoudiniRuntimeSettings>();
	if (HoudiniRuntimeSettings)
		bDisplaySlateCookingNotifications = HoudiniRuntimeSettings->bDisplaySlateCookingNotifications;

	if (!bDisplaySlateCookingNotifications)
		return;

	FText NotificationText = FText::FromString(NotificationString);
	FNotificationInfo Info(NotificationText);

	Info.bFireAndForget = true;
	Info.FadeOutDuration = NotificationFadeOut;
	Info.ExpireDuration = NotificationExpire;

	TSharedPtr<FSlateDynamicImageBrush> HoudiniBrush = FHoudiniEngine::Get().GetHoudiniLogoBrush();
	if (HoudiniBrush.IsValid())
		Info.Image = HoudiniBrush.Get();

	FSlateNotificationManager::Get().AddNotification(Info);
#endif

	return;
}

FString
FHoudiniEngineUtils::GetHoudiniEnginePluginDir()
{
	FString EnginePluginDir = FPaths::EnginePluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(EnginePluginDir))
		return EnginePluginDir;

	FString ProjectPluginDir = FPaths::ProjectPluginsDir() / TEXT("Runtime/HoudiniEngine");
	if (FPaths::DirectoryExists(ProjectPluginDir))
		return ProjectPluginDir;

	TSharedPtr<IPlugin> HoudiniPlugin = IPluginManager::Get().FindPlugin(TEXT("HoudiniEngine"));
	FString PluginBaseDir = HoudiniPlugin.IsValid() ? HoudiniPlugin->GetBaseDir() : EnginePluginDir;
	if (FPaths::DirectoryExists(PluginBaseDir))
		return PluginBaseDir;

	HOUDINI_LOG_WARNING(TEXT("Could not find the Houdini Engine plugin's directory"));

	return EnginePluginDir;
}


HAPI_Result
FHoudiniEngineUtils::CreateNode(
	const HAPI_NodeId& InParentNodeId,
	const char * operator_name,
	const char * node_label,
	const HAPI_Bool& bInCookOnCreation,
	HAPI_NodeId* OutNewNodeId)
{
	// Call HAPI::CreateNode
	HAPI_Result Result = FHoudiniApi::CreateNode(
		FHoudiniEngine::Get().GetSession(),
		InParentNodeId, operator_name, node_label, bInCookOnCreation, OutNewNodeId);

	// Return now if CreateNode fialed
	if (Result != HAPI_RESULT_SUCCESS)
		return Result;
		
	// Loop on the cook_state status until it's ready
	int CurrentStatus = HAPI_State::HAPI_STATE_STARTING_LOAD;
	while (CurrentStatus > HAPI_State::HAPI_STATE_MAX_READY_STATE)
	{
		if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetStatus(
			FHoudiniEngine::Get().GetSession(),
			HAPI_StatusType::HAPI_STATUS_COOK_STATE, &CurrentStatus))
		{
			// Exit the loop if GetStatus somehow fails
			break;
		}
	}

	if (CurrentStatus == HAPI_STATE_READY_WITH_FATAL_ERRORS)
	{
		// Fatal errors - failed
		HOUDINI_LOG_ERROR(TEXT("Failed to create node %s - %s"), operator_name, node_label);
		return HAPI_RESULT_FAILURE;
	}
	else if (CurrentStatus == HAPI_STATE_READY_WITH_COOK_ERRORS)
	{
		// Mention the errors - still return success
		HOUDINI_LOG_WARNING(TEXT("Errors when creating node %s - %s"), operator_name, node_label);
	}

	return HAPI_RESULT_SUCCESS;
}


int32
FHoudiniEngineUtils::HapiGetCookCount(const HAPI_NodeId& InNodeId)
{
	// TODO:
	// Use HAPI_GetCookingTotalCount() when available
	HAPI_NodeInfo NodeInfo;
	FHoudiniApi::NodeInfo_Init(&NodeInfo);

	int32 CookCount = -1;
	HAPI_Result Result = FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), InNodeId, &NodeInfo);
	
	if (Result != HAPI_RESULT_FAILURE)
	{
		if (NodeInfo.type != HAPI_NODETYPE_OBJ)
		{
			// For SOP assets, get the cook count straight from the Asset Node
			CookCount = NodeInfo.totalCookCount;
		}
		else
		{
			// For OBJ nodes, get the cook count from the display geos
			// Retrieve information about each object contained within our asset.
			TArray< HAPI_ObjectInfo > ObjectInfos;
			if (!FHoudiniEngineUtils::HapiGetObjectInfos(InNodeId, ObjectInfos))
				return false;

			for (auto CurrentHapiObjectInfo : ObjectInfos)
			{
				// Get the Display Geo's info				
				HAPI_GeoInfo DisplayHapiGeoInfo;
				FHoudiniApi::GeoInfo_Init(&DisplayHapiGeoInfo);
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetDisplayGeoInfo(
					FHoudiniEngine::Get().GetSession(), CurrentHapiObjectInfo.nodeId, &DisplayHapiGeoInfo))
				{
					continue;
				}

				HAPI_NodeInfo DisplayNodeInfo;
				FHoudiniApi::NodeInfo_Init(&DisplayNodeInfo);
				if (HAPI_RESULT_SUCCESS != FHoudiniApi::GetNodeInfo(FHoudiniEngine::Get().GetSession(), DisplayHapiGeoInfo.nodeId, &DisplayNodeInfo))
				{
					continue;
				}

				CookCount += DisplayNodeInfo.totalCookCount;
			}
		}
	}		

	return CookCount;
}