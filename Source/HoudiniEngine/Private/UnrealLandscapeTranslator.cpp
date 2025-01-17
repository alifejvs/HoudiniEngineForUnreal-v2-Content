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

#include "HoudiniApi.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngine.h"
#include "HoudiniRuntimeSettings.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineString.h"

#include "UnrealLandscapeTranslator.h"
#include "HoudiniGeoPartObject.h"

#include "Landscape.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LightMap.h"
#include "Engine/MapBuildDataRegistry.h"
#include "PhysicalMaterials/PhysicalMaterial.h"


bool 
FUnrealLandscapeTranslator::CreateMeshOrPointsFromLandscape(
	ALandscapeProxy* LandscapeProxy, 
	HAPI_NodeId& CreatedNodeId, 
	const FString& InputNodeNameString,
	const bool& bExportGeometryAsMesh, 
	const bool& bExportTileUVs,
	const bool bExportNormalizedUVs,
	const bool bExportLighting,
	const bool bExportMaterials	)
{
	//--------------------------------------------------------------------------------------------------
	// 1. Create an input node
    //--------------------------------------------------------------------------------------------------
	HAPI_NodeId InputNodeId = -1;
	// Create the curve SOP Node
	std::string NodeNameRawString;
	FHoudiniEngineUtils::ConvertUnrealString(InputNodeNameString, NodeNameRawString);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateInputNode(
		FHoudiniEngine::Get().GetSession(), &InputNodeId, NodeNameRawString.c_str()), false);

	// Check if we have a valid id for this new input asset.
	if (!FHoudiniEngineUtils::IsHoudiniNodeValid(InputNodeId))
		return false;

	// We now have a valid id.
	CreatedNodeId = InputNodeId;

	if(!FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true))
		return false;
	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), InputNodeId, &CookOptions), false);
	*/
	//--------------------------------------------------------------------------------------------------
    // 2. Set the part info
    //--------------------------------------------------------------------------------------------------
	int32 ComponentSizeQuads = ((LandscapeProxy->ComponentSizeQuads + 1) >> LandscapeProxy->ExportLOD) - 1;
	float ScaleFactor = (float)LandscapeProxy->ComponentSizeQuads / (float)ComponentSizeQuads;

	//int32 NumComponents = bExportOnlySelected ? SelectedComponents.Num() : LandscapeProxy->LandscapeComponents.Num();
	int32 NumComponents = LandscapeProxy->LandscapeComponents.Num();
	int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	int32 VertexCount = NumComponents * VertexCountPerComponent;
	if (!VertexCount)
		return false;

	int32 TriangleCount = NumComponents * FMath::Square(ComponentSizeQuads) * 2;
	int32 QuadCount = NumComponents * FMath::Square(ComponentSizeQuads);
	int32 IndexCount = QuadCount * 4;

	// Create part info
	HAPI_PartInfo Part;
	FHoudiniApi::PartInfo_Init(&Part);
	//FMemory::Memzero< HAPI_PartInfo >(Part);
	Part.id = 0;
	Part.nameSH = 0;
	Part.attributeCounts[HAPI_ATTROWNER_POINT] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_PRIM] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_VERTEX] = 0;
	Part.attributeCounts[HAPI_ATTROWNER_DETAIL] = 0;
	Part.vertexCount = 0;
	Part.faceCount = 0;
	Part.pointCount = VertexCount;
	Part.type = HAPI_PARTTYPE_MESH;

	// If we are exporting to a mesh, we need vertices and faces
	if (bExportGeometryAsMesh)
	{
		Part.vertexCount = IndexCount;
		Part.faceCount = QuadCount;
	}

	// Set the part infos
	HAPI_GeoInfo DisplayGeoInfo;
	FHoudiniApi::GeoInfo_Init(&DisplayGeoInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetDisplayGeoInfo(
		FHoudiniEngine::Get().GetSession(), CreatedNodeId, &DisplayGeoInfo), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetPartInfo(
		FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId, 0, &Part), false);

	//--------------------------------------------------------------------------------------------------
	// 3. Extract the landscape data
	//--------------------------------------------------------------------------------------------------
	// Array for the position data
	TArray<FVector> LandscapePositionArray;
	// Array for the normals
	TArray<FVector> LandscapeNormalArray;
	// Array for the UVs
	TArray<FVector> LandscapeUVArray;
	// Array for the vertex index of each point in its component
	TArray<FIntPoint> LandscapeComponentVertexIndicesArray;
	// Array for the tile names per point
	TArray<const char *> LandscapeComponentNameArray;
	// Array for the lightmap values
	TArray<FLinearColor> LandscapeLightmapValues;
	// Selected components set to all components in current landscape proxy
	TSet<ULandscapeComponent*> SelectedComponents;
	SelectedComponents.Append(LandscapeProxy->LandscapeComponents);

	// Extract all the data from the landscape to the arrays
	if (!ExtractLandscapeData(
		LandscapeProxy, SelectedComponents,
		bExportLighting, bExportTileUVs, bExportNormalizedUVs,
		LandscapePositionArray, LandscapeNormalArray,
		LandscapeUVArray, LandscapeComponentVertexIndicesArray,
		LandscapeComponentNameArray, LandscapeLightmapValues))
		return false;

	//--------------------------------------------------------------------------------------------------
    // 3. Set the corresponding attributes in Houdini
    //--------------------------------------------------------------------------------------------------

    // Create point attribute info containing positions.
	if (!AddLandscapePositionAttribute(DisplayGeoInfo.nodeId, LandscapePositionArray))
		return false;

	// Create point attribute info containing normals.
	if (!AddLandscapeNormalAttribute(DisplayGeoInfo.nodeId, LandscapeNormalArray))
		return false;

	// Create point attribute info containing UVs.
	if (!AddLandscapeUVAttribute(DisplayGeoInfo.nodeId, LandscapeUVArray))
		return false;

	// Create point attribute containing landscape component vertex indices (indices of vertices within the grid - x,y).
	if (!AddLandscapeComponentVertexIndicesAttribute(DisplayGeoInfo.nodeId, LandscapeComponentVertexIndicesArray))
		return false;

	// Create point attribute containing landscape component name.
	if (!AddLandscapeComponentNameAttribute(DisplayGeoInfo.nodeId, LandscapeComponentNameArray))
		return false;

	// Create point attribute info containing lightmap information.
	if (bExportLighting)
	{
		if (!AddLandscapeLightmapColorAttribute(DisplayGeoInfo.nodeId, LandscapeLightmapValues))
			return false;
	}

	// Set indices if we are exporting full geometry.
	if (bExportGeometryAsMesh)
	{
		if (!AddLandscapeMeshIndicesAndMaterialsAttribute(
			DisplayGeoInfo.nodeId,
			bExportMaterials,
			ComponentSizeQuads,
			QuadCount,
			LandscapeProxy,
			SelectedComponents))
			return false;
	}

	// If we are marshalling material information.
	if (bExportMaterials)
	{
		if (!AddLandscapeGlobalMaterialAttribute(DisplayGeoInfo.nodeId, LandscapeProxy))
			return false;
	}

	/*
	// TODO: Move this to ExtractLandscapeData()
	//--------------------------------------------------------------------------------------------------
	// 4. Extract and convert all the layers
	//--------------------------------------------------------------------------------------------------
	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Get the landscape X/Y Size
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;
	if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		return false;

	// Calc the X/Y size in points
	int32 XSize = (MaxX - MinX + 1);
	int32 YSize = (MaxY - MinY + 1);
	if ((XSize < 2) || (YSize < 2))
		return false;

	bool MaskInitialized = false;
	int32 NumLayers = LandscapeInfo->Layers.Num();
	for (int32 n = 0; n < NumLayers; n++)
	{
		// 1. Extract the uint8 values from the layer
		TArray<uint8> CurrentLayerIntData;
		FLinearColor LayerUsageDebugColor;
		FString LayerName;
		if (!GetLandscapeLayerData(
			LandscapeInfo, n,
			MinX, MinY, MaxX, MaxY,
			CurrentLayerIntData, LayerUsageDebugColor, LayerName))
			continue;

		// 2. Convert unreal uint8 values to floats
		// If the layer came from Houdini, additional info might have been stored in the DebugColor to convert the data back to float
		HAPI_VolumeInfo CurrentLayerVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentLayerVolumeInfo);
		TArray<float> CurrentLayerFloatData;
		if (!ConvertLandscapeLayerDataToHeightfieldData(
			CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
			CurrentLayerFloatData, CurrentLayerVolumeInfo))
			continue;

		if (!AddLandscapeLayerAttribute(
			DisplayGeoInfo.nodeId, CurrentLayerFloatData, LayerName))
			continue;
	}
	*/

	// Commit the geo.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), DisplayGeoInfo.nodeId), false);

	// TODO: Remove me!
	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), InputNodeId, &CookOptions), false);
	*/

	return FHoudiniEngineUtils::HapiCookNode(InputNodeId, nullptr, true);
}

bool 
FUnrealLandscapeTranslator::CreateHeightfieldFromLandscape(
	ALandscapeProxy* LandscapeProxy, HAPI_NodeId& CreatedHeightfieldNodeId, const FString& InputNodeNameStr) 
{
	if (!LandscapeProxy)
		return false;

	// Export the whole landscape and its layer as a single heightfield.

	//--------------------------------------------------------------------------------------------------
	// 1. Extracting the height data
	//--------------------------------------------------------------------------------------------------
	TArray<uint16> HeightData;
	int32 XSize, YSize;
	FVector Min, Max;
	if (!GetLandscapeData(LandscapeProxy, HeightData, XSize, YSize, Min, Max))
		return false;

	//--------------------------------------------------------------------------------------------------
	// 2. Convert the height uint16 data to float
	//--------------------------------------------------------------------------------------------------
	TArray<float> HeightfieldFloatValues;
	HAPI_VolumeInfo HeightfieldVolumeInfo;
	FHoudiniApi::VolumeInfo_Init(&HeightfieldVolumeInfo);
	FTransform LandscapeTransform = LandscapeProxy->ActorToWorld();
	FVector CenterOffset = FVector::ZeroVector;
	if (!ConvertLandscapeDataToHeightfieldData(
		HeightData, XSize, YSize, Min, Max, LandscapeTransform,
		HeightfieldFloatValues, HeightfieldVolumeInfo, CenterOffset))
		return false;

	//--------------------------------------------------------------------------------------------------
	// 3. Create the Heightfield Input Node
	//-------------------------------------------------------------------------------------------------- 
	HAPI_NodeId HeightFieldId = -1;
	HAPI_NodeId HeightId = -1;
	HAPI_NodeId MaskId = -1;
	HAPI_NodeId MergeId = -1;
	if (!CreateHeightfieldInputNode(InputNodeNameStr, XSize, YSize, HeightFieldId, HeightId, MaskId, MergeId))
		return false;

	//--------------------------------------------------------------------------------------------------
	// 4. Set the HeightfieldData in Houdini
	//--------------------------------------------------------------------------------------------------    
	// Set the Height volume's data
	HAPI_PartId PartId = 0;
	if (!SetHeightfieldData(HeightId, PartId, HeightfieldFloatValues, HeightfieldVolumeInfo, TEXT("height")))
		return false;

	// Add the materials used
	UMaterialInterface* LandscapeMat = LandscapeProxy->GetLandscapeMaterial();
	UMaterialInterface* LandscapeHoleMat = LandscapeProxy->GetLandscapeHoleMaterial();
	UPhysicalMaterial* LandscapePhysMat = LandscapeProxy->DefaultPhysMaterial;
	AddLandscapeMaterialAttributesToVolume(HeightId, PartId, LandscapeMat, LandscapeHoleMat, LandscapePhysMat);

	// Add the landscape's actor tags as prim attributes if we have any    
	FHoudiniEngineUtils::CreateAttributesFromTags(HeightId, PartId, LandscapeProxy->Tags);

	// Add the unreal_actor_path attribute
	FHoudiniEngineUtils::AddActorPathAttribute(HeightId, PartId, LandscapeProxy, 1);

	// Add the unreal_level_path attribute
	ULevel* Level = LandscapeProxy->GetLevel();
	if (Level)
	{
		FHoudiniEngineUtils::AddLevelPathAttribute(HeightId, PartId, Level, 1);
		/*
		LevelPath = Level->GetPathName();

		// We just want the path up to the first point
		int32 DotIndex;
		if (LevelPath.FindChar('.', DotIndex))
			LevelPath.LeftInline(DotIndex, false);

		AddLevelPathAttributeToVolume(HeightId, PartId, LevelPath);
		*/
	}

	// Commit the height volume
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
		FHoudiniEngine::Get().GetSession(), HeightId), false);

	//--------------------------------------------------------------------------------------------------
    // 5. Extract and convert all the layers
    //--------------------------------------------------------------------------------------------------
	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	bool MaskInitialized = false;
	int32 MergeInputIndex = 2;
	int32 NumLayers = LandscapeInfo->Layers.Num();
	for (int32 n = 0; n < NumLayers; n++)
	{
		// 1. Extract the uint8 values from the layer
		TArray<uint8> CurrentLayerIntData;
		FLinearColor LayerUsageDebugColor;
		FString LayerName;
		if (!GetLandscapeLayerData(LandscapeInfo, n, CurrentLayerIntData, LayerUsageDebugColor, LayerName))
			continue;

		// 2. Convert unreal uint8 values to floats
		// If the layer came from Houdini, additional info might have been stored in the DebugColor to convert the data back to float
		HAPI_VolumeInfo CurrentLayerVolumeInfo;
		FHoudiniApi::VolumeInfo_Init(&CurrentLayerVolumeInfo);
		TArray<float> CurrentLayerFloatData;
		if (!ConvertLandscapeLayerDataToHeightfieldData(
			CurrentLayerIntData, XSize, YSize, LayerUsageDebugColor,
			CurrentLayerFloatData, CurrentLayerVolumeInfo))
			continue;

		// We reuse the height layer's transform
		CurrentLayerVolumeInfo.transform = HeightfieldVolumeInfo.transform;

		// 3. See if we need to create an input volume, or can reuse the HF's default mask volume
		bool IsMask = false;
		if (LayerName.Equals(TEXT("mask"), ESearchCase::IgnoreCase))
			IsMask = true;

		HAPI_NodeId LayerVolumeNodeId = -1;
		if (!IsMask)
		{
			// Current layer is not mask, so we need to create a new input volume
			std::string LayerNameStr;
			FHoudiniEngineUtils::ConvertUnrealString(LayerName, LayerNameStr);

			FHoudiniApi::CreateHeightfieldInputVolumeNode(
				FHoudiniEngine::Get().GetSession(),
				HeightFieldId, &LayerVolumeNodeId, LayerNameStr.c_str(), XSize, YSize, 1.0f);
		}
		else
		{
			// Current Layer is mask, so we simply reuse the mask volume node created by default by the heightfield node
			LayerVolumeNodeId = MaskId;
		}

		// Check if we have a valid id for the input volume.
		if (!FHoudiniEngineUtils::IsHoudiniNodeValid(LayerVolumeNodeId))
			continue;

		// 4. Set the layer/mask heighfield data in Houdini
		HAPI_PartId CurrentPartId = 0;
		if (!SetHeightfieldData(LayerVolumeNodeId, PartId, CurrentLayerFloatData, CurrentLayerVolumeInfo, LayerName))
			continue;

		// Get the physical material used by that layer
		UPhysicalMaterial* LayerPhysicalMat = LandscapePhysMat;
		{
			FLandscapeInfoLayerSettings LayersSetting = LandscapeInfo->Layers[n];
			ULandscapeLayerInfoObject* LayerInfo = LayersSetting.LayerInfoObj;
			if (LayerInfo)
				LayerPhysicalMat = LayerInfo->PhysMaterial;
		}

		// Also add the material attributes to the layer volumes
		AddLandscapeMaterialAttributesToVolume(LayerVolumeNodeId, PartId, LandscapeMat, LandscapeHoleMat, LayerPhysicalMat);

		// Add the landscape's actor tags as prim attributes if we have any    
		FHoudiniEngineUtils::CreateAttributesFromTags(LayerVolumeNodeId, PartId, LandscapeProxy->Tags);

		// Add the unreal_actor_path attribute
		FHoudiniEngineUtils::AddActorPathAttribute(LayerVolumeNodeId, PartId, LandscapeProxy, 1);

		// Also add the level path attribute
		FHoudiniEngineUtils::AddLevelPathAttribute(LayerVolumeNodeId, PartId, Level, 1);
		//AddLevelPathAttributeToVolume(LayerVolumeNodeId, PartId, LevelPath);

		// Commit the volume's geo
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), LayerVolumeNodeId), false);

		if (!IsMask)
		{
			// We had to create a new volume for this layer, so we need to connect it to the HF's merge node
			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::ConnectNodeInput(
				FHoudiniEngine::Get().GetSession(),
				MergeId, MergeInputIndex, LayerVolumeNodeId, 0), false);

			MergeInputIndex++;
		}
		else
		{
			MaskInitialized = true;
		}
	}

	// We need to have a mask layer as it is required for proper heightfield functionalities
	// Setting the volume info on the mask is needed for the HF to have proper transform in H!
	// If we didn't create a mask volume before, send a default one now
	if (!MaskInitialized)
	{
		MaskInitialized = InitDefaultHeightfieldMask(HeightfieldVolumeInfo, MaskId);

		// Add the materials used
		AddLandscapeMaterialAttributesToVolume(MaskId, PartId, LandscapeMat, LandscapeHoleMat, LandscapePhysMat);

		// Add the landscape's actor tags as prim attributes if we have any    
		FHoudiniEngineUtils::CreateAttributesFromTags(MaskId, PartId, LandscapeProxy->Tags);

		// Add the unreal_actor_path attribute
		FHoudiniEngineUtils::AddActorPathAttribute(MaskId, PartId, LandscapeProxy, 1);

		// Also add the level path attribute
		FHoudiniEngineUtils::AddLevelPathAttribute(MaskId, PartId, Level, 1);
		//AddLevelPathAttributeToVolume(MaskId, PartId, LevelPath);

		// Commit the mask volume's geo
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CommitGeo(
			FHoudiniEngine::Get().GetSession(), MaskId), false);
	}

	HAPI_TransformEuler HAPIObjectTransform;
	FHoudiniApi::TransformEuler_Init(&HAPIObjectTransform);
	//FMemory::Memzero< HAPI_TransformEuler >( HAPIObjectTransform );
	LandscapeTransform.SetScale3D(FVector::OneVector);
	FHoudiniEngineUtils::TranslateUnrealTransform(LandscapeTransform, HAPIObjectTransform);
	HAPIObjectTransform.position[1] = 0.0f;

	HAPI_NodeId ParentObjNodeId = FHoudiniEngineUtils::HapiGetParentNodeId(HeightFieldId);
	FHoudiniApi::SetObjectTransform(FHoudiniEngine::Get().GetSession(), ParentObjNodeId, &HAPIObjectTransform);

	// Since HF are centered but landscape aren't, we need to set the HF's center parameter
	FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 0, CenterOffset.X);
	FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 1, 0.0);
	FHoudiniApi::SetParmFloatValue(FHoudiniEngine::Get().GetSession(), HeightFieldId, "t", 2, CenterOffset.Y);

	// Finally, cook the Heightfield node
	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), HeightFieldId, &CookOptions), false);
	*/
	if(!FHoudiniEngineUtils::HapiCookNode(HeightFieldId, nullptr, true))
		return false;

	CreatedHeightfieldNodeId = HeightFieldId;

	return true;
}

// Converts Unreal uint16 values to Houdini Float
bool
FUnrealLandscapeTranslator::ConvertLandscapeLayerDataToHeightfieldData(
	const TArray<uint8>& IntHeightData,
	const int32& XSize, const int32& YSize,
	const FLinearColor& LayerUsageDebugColor,
	TArray<float>& LayerFloatValues,
	HAPI_VolumeInfo& LayerVolumeInfo)
{
	LayerFloatValues.Empty();

	int32 HoudiniXSize = YSize;
	int32 HoudiniYSize = XSize;
	int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
	if ((HoudiniXSize < 2) || (HoudiniYSize < 2))
		return false;

	if (IntHeightData.Num() != SizeInPoints)
		return false;

	//--------------------------------------------------------------------------------------------------
	// 1. Convert values to float
	//--------------------------------------------------------------------------------------------------

	// By default, values are converted from unreal [0 255] uint8 to Houdini [0 1] float	
	// uint8 min/max
	uint8 IntMin = 0;
	uint8 IntMax = UINT8_MAX;
	// The range in Digits	
	double DigitRange = (double)UINT8_MAX;

	// By default, the values will be converted to [0, 1]
	float LayerMin = 0.0f;
	float LayerMax = 1.0f;
	float LayerSpacing = 1.0f / DigitRange;
	
	// If this layer came from Houdini, its alpha value should be PI
	// This indicates that we can extract additional infos stored its debug usage color
	// so we can reconstruct the original source values (float) more accurately
	if (LayerUsageDebugColor.A == PI)
	{
		// We need the ZMin / ZMax uint8 values
		IntMin = IntHeightData[0];
		IntMax = IntMin;
		for (int n = 0; n < IntHeightData.Num(); n++)
		{
			if (IntHeightData[n] < IntMin)
				IntMin = IntHeightData[n];
			if (IntHeightData[n] > IntMax)
				IntMax = IntHeightData[n];
		}

		DigitRange = (double)IntMax - (double)IntMin;

		// Read the original min/max and spacing stored in the debug color
		LayerMin = LayerUsageDebugColor.R;
		LayerMax = LayerUsageDebugColor.G;
		LayerSpacing = LayerUsageDebugColor.B;
	}

	// Convert the Int data to Float
	LayerFloatValues.SetNumUninitialized(SizeInPoints);

	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// We need to invert X/Y when reading the value from Unreal
			int32 nHoudini = nX + nY * HoudiniXSize;
			int32 nUnreal = nY + nX * XSize;

			// Convert the int values to meter
			// Unreal's digit value have a zero value of 32768
			double DoubleValue = ((double)IntHeightData[nUnreal] - (double)IntMin) * LayerSpacing + LayerMin;
			LayerFloatValues[nHoudini] = (float)DoubleValue;
		}
	}

	/*
	// Verifying the converted ZMin / ZMax
	float FloatMin = LayerFloatValues[0];
	float FloatMax = FloatMin;
	for (int32 n = 0; n < LayerFloatValues.Num(); n++)
	{
		if (LayerFloatValues[n] < FloatMin)
			FloatMin = LayerFloatValues[n];
		if (LayerFloatValues[n] > FloatMax)
			FloatMax = LayerFloatValues[n];
	}
	*/

	//--------------------------------------------------------------------------------------------------
	// 2. Fill the volume info
	//--------------------------------------------------------------------------------------------------
	LayerVolumeInfo.xLength = HoudiniXSize;
	LayerVolumeInfo.yLength = HoudiniYSize;
	LayerVolumeInfo.zLength = 1;

	LayerVolumeInfo.minX = 0;
	LayerVolumeInfo.minY = 0;
	LayerVolumeInfo.minZ = 0;

	LayerVolumeInfo.type = HAPI_VOLUMETYPE_HOUDINI;
	LayerVolumeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	LayerVolumeInfo.tupleSize = 1;
	LayerVolumeInfo.tileSize = 1;

	LayerVolumeInfo.hasTaper = false;
	LayerVolumeInfo.xTaper = 0.0;
	LayerVolumeInfo.yTaper = 0.0;

	// The layer transform will have to be copied from the main heightfield's transform
	return true;
}

bool
FUnrealLandscapeTranslator::GetLandscapeData(
	ALandscapeProxy* LandscapeProxy,
	TArray<uint16>& HeightData,
	int32& XSize, int32& YSize,
	FVector& Min, FVector& Max)
{
	if (!LandscapeProxy)
		return false;

	ULandscapeInfo* LandscapeInfo = LandscapeProxy->GetLandscapeInfo();
	if (!LandscapeInfo)
		return false;

	// Get the landscape extents to get its size
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;

	// To handle streaming proxies correctly, get the extents via all the components,
	// not by calling GetLandscapeExtent or we'll end up sending ALL the streaming proxies.
	for (const ULandscapeComponent* Comp : LandscapeProxy->LandscapeComponents)
	{
		Comp->GetComponentExtent(MinX, MinY, MaxX, MaxY);
	}

	if (!GetLandscapeData(LandscapeInfo, MinX, MinY, MaxX, MaxY, HeightData, XSize, YSize))
		return false;

	// Get the landscape Min/Max values
	// Do not use Landscape->GetActorBounds() here as instanced geo
	// (due to grass layers for example) can cause it to return incorrect bounds!
	FVector Origin, Extent;
	GetLandscapeProxyBounds(LandscapeProxy, Origin, Extent);

	// Get the landscape Min/Max values
	Min = Origin - Extent;
	Max = Origin + Extent;

	return true;
}

bool
FUnrealLandscapeTranslator::GetLandscapeData(
	ULandscapeInfo* LandscapeInfo,
	const int32& MinX, const int32& MinY,
	const int32& MaxX, const int32& MaxY,
	TArray<uint16>& HeightData,
	int32& XSize, int32& YSize)
{
	if (!LandscapeInfo)
		return false;

	// Get the X/Y size in points
	XSize = (MaxX - MinX + 1);
	YSize = (MaxY - MinY + 1);

	if ((XSize < 2) || (YSize < 2))
		return false;

	// Extracting the uint16 values from the landscape
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	HeightData.AddZeroed(XSize * YSize);
	LandscapeEdit.GetHeightDataFast(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

	return true;
}


void
FUnrealLandscapeTranslator::GetLandscapeProxyBounds(
	ALandscapeProxy* LandscapeProxy, FVector& Origin, FVector& Extents)
{
	// Iterate only on the landscape components
	FBox Bounds(ForceInit);
	for (const UActorComponent* ActorComponent : LandscapeProxy->GetComponents())
	{
		const ULandscapeComponent* LandscapeComp = Cast<const ULandscapeComponent>(ActorComponent);
		if (LandscapeComp && LandscapeComp->IsRegistered())
			Bounds += LandscapeComp->Bounds.GetBox();
	}

	// Convert the bounds to origin/offset vectors
	Bounds.GetCenterAndExtents(Origin, Extents);
}

bool
FUnrealLandscapeTranslator::ConvertLandscapeDataToHeightfieldData(
	const TArray<uint16>& IntHeightData,
	const int32& XSize, const int32& YSize,
	FVector Min, FVector Max,
	const FTransform& LandscapeTransform,
	TArray<float>& HeightfieldFloatValues,
	HAPI_VolumeInfo& HeightfieldVolumeInfo,
	FVector& CenterOffset)
{
	HeightfieldFloatValues.Empty();

	int32 HoudiniXSize = YSize;
	int32 HoudiniYSize = XSize;
	int32 SizeInPoints = HoudiniXSize * HoudiniYSize;
	if ((HoudiniXSize < 2) || (HoudiniYSize < 2))
		return false;

	if (IntHeightData.Num() != SizeInPoints)
		return false;

	// Use default unreal scaling for marshalling landscapes
	// A lot of precision will be lost in order to keep the same transform as the landscape input
	bool bUseDefaultUE4Scaling = false;
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();
	if (HoudiniRuntimeSettings && HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling)
		bUseDefaultUE4Scaling = HoudiniRuntimeSettings->MarshallingLandscapesUseDefaultUnrealScaling;

	//--------------------------------------------------------------------------------------------------
	// 1. Convert values to float
	//--------------------------------------------------------------------------------------------------


	// Convert the min/max values from cm to meters
	Min /= 100.0;
	Max /= 100.0;

	// Unreal's landscape uses 16bits precision and range from -256m to 256m with the default scale of 100.0
	// To convert the uint16 values to float "metric" values, offset the int by 32768 to center it,
	// then scale it

	// Spacing used to convert from uint16 to meters
	double ZSpacing = 512.0 / ((double)UINT16_MAX);
	ZSpacing *= ((double)LandscapeTransform.GetScale3D().Z / 100.0);

	// Center value in meters (Landscape ranges from [-255:257] meters at default scale
	double ZCenterOffset = 32767;
	double ZPositionOffset = LandscapeTransform.GetLocation().Z / 100.0f;
	// Convert the Int data to Float
	HeightfieldFloatValues.SetNumUninitialized(SizeInPoints);

	for (int32 nY = 0; nY < HoudiniYSize; nY++)
	{
		for (int32 nX = 0; nX < HoudiniXSize; nX++)
		{
			// We need to invert X/Y when reading the value from Unreal
			int32 nHoudini = nX + nY * HoudiniXSize;
			int32 nUnreal = nY + nX * XSize;

			// Convert the int values to meter
			// Unreal's digit value have a zero value of 32768
			double DoubleValue = ((double)IntHeightData[nUnreal] - ZCenterOffset) * ZSpacing + ZPositionOffset;
			HeightfieldFloatValues[nHoudini] = (float)DoubleValue;
		}
	}

	//--------------------------------------------------------------------------------------------------
	// 2. Convert the Unreal Transform to a HAPI_transform
	//--------------------------------------------------------------------------------------------------
	HAPI_Transform HapiTransform;
	FHoudiniApi::Transform_Init(&HapiTransform);
	//FMemory::Memzero< HAPI_Transform >( HapiTransform );
	{
		FQuat Rotation = LandscapeTransform.GetRotation();
		if (Rotation != FQuat::Identity)
		{
			//Swap(ObjectRotation.Y, ObjectRotation.Z);
			HapiTransform.rotationQuaternion[0] = Rotation.X;
			HapiTransform.rotationQuaternion[1] = Rotation.Z;
			HapiTransform.rotationQuaternion[2] = Rotation.Y;
			HapiTransform.rotationQuaternion[3] = -Rotation.W;
		}
		else
		{
			HapiTransform.rotationQuaternion[0] = 0;
			HapiTransform.rotationQuaternion[1] = 0;
			HapiTransform.rotationQuaternion[2] = 0;
			HapiTransform.rotationQuaternion[3] = 1;
		}

		// Heightfield are centered, landscapes are not
		CenterOffset = (Max - Min) * 0.5f;

		// Unreal XYZ becomes Houdini YXZ (since heightfields are also rotated due the ZX transform) 
		//FVector Position = LandscapeTransform.GetLocation() / 100.0f;
		HapiTransform.position[1] = 0.0f;//Position.X + CenterOffset.X;
		HapiTransform.position[0] = 0.0f;//Position.Y + CenterOffset.Y;
		HapiTransform.position[2] = 0.0f;

		FVector Scale = LandscapeTransform.GetScale3D() / 100.0f;
		HapiTransform.scale[0] = Scale.X * 0.5f * HoudiniXSize;
		HapiTransform.scale[1] = Scale.Y * 0.5f * HoudiniYSize;
		HapiTransform.scale[2] = 0.5f;
		if (bUseDefaultUE4Scaling)
			HapiTransform.scale[2] *= Scale.Z;

		HapiTransform.shear[0] = 0.0f;
		HapiTransform.shear[1] = 0.0f;
		HapiTransform.shear[2] = 0.0f;
	}

	//--------------------------------------------------------------------------------------------------
	// 3. Fill the volume info
	//--------------------------------------------------------------------------------------------------
	HeightfieldVolumeInfo.xLength = HoudiniXSize;
	HeightfieldVolumeInfo.yLength = HoudiniYSize;
	HeightfieldVolumeInfo.zLength = 1;

	HeightfieldVolumeInfo.minX = 0;
	HeightfieldVolumeInfo.minY = 0;
	HeightfieldVolumeInfo.minZ = 0;

	HeightfieldVolumeInfo.transform = HapiTransform;

	HeightfieldVolumeInfo.type = HAPI_VOLUMETYPE_HOUDINI;
	HeightfieldVolumeInfo.storage = HAPI_STORAGETYPE_FLOAT;
	HeightfieldVolumeInfo.tupleSize = 1;
	HeightfieldVolumeInfo.tileSize = 1;

	HeightfieldVolumeInfo.hasTaper = false;
	HeightfieldVolumeInfo.xTaper = 0.0;
	HeightfieldVolumeInfo.yTaper = 0.0;

	return true;
}

bool
FUnrealLandscapeTranslator::CreateHeightfieldInputNode(
	const FString& NodeName,
	const int32& XSize,
	const int32& YSize,
	HAPI_NodeId& HeightfieldNodeId, 
	HAPI_NodeId& HeightNodeId, 
	HAPI_NodeId& MaskNodeId, 
	HAPI_NodeId& MergeNodeId)
{
	// Make sure the Heightfield node doesnt already exists
	if (HeightfieldNodeId != -1)
		return false;

	// Convert the node's name
	std::string NameStr;
	FHoudiniEngineUtils::ConvertUnrealString(NodeName, NameStr);

	// Create the heigthfield node via HAPI
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CreateHeightFieldInput(
		FHoudiniEngine::Get().GetSession(),
		-1, NameStr.c_str(), XSize, YSize, 1.0f, HAPI_HeightFieldSampling::HAPI_HEIGHTFIELD_SAMPLING_CORNER,
		&HeightfieldNodeId, &HeightNodeId, &MaskNodeId, &MergeNodeId), false);
	
	// Cook it
	return FHoudiniEngineUtils::HapiCookNode(HeightfieldNodeId, nullptr, true);
	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), HeightfieldNodeId, &CookOptions), false);

	return true;
	*/
}

bool
FUnrealLandscapeTranslator::SetHeightfieldData(
	const HAPI_NodeId& VolumeNodeId,
	const HAPI_PartId& PartId,
	TArray<float>& FloatValues,
	const HAPI_VolumeInfo& VolumeInfo,
	const FString& HeightfieldName)
{
	// Cook the node to get proper infos on it
	/*
	HAPI_CookOptions CookOptions = FHoudiniEngine::GetDefaultCookOptions();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::CookNode(
		FHoudiniEngine::Get().GetSession(), VolumeNodeId, &CookOptions), false);
	*/
	if(!FHoudiniEngineUtils::HapiCookNode(VolumeNodeId, nullptr, true))
		return false;

	// Read the geo/part/volume info from the volume node
	HAPI_GeoInfo GeoInfo;
	FHoudiniApi::GeoInfo_Init(&GeoInfo);
	//FMemory::Memset< HAPI_GeoInfo >(GeoInfo, 0);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetGeoInfo(
		FHoudiniEngine::Get().GetSession(),
		VolumeNodeId, &GeoInfo), false);

	HAPI_PartInfo PartInfo;
	FHoudiniApi::PartInfo_Init(&PartInfo);
	//FMemory::Memset< HAPI_PartInfo >(PartInfo, 0);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetPartInfo(
		FHoudiniEngine::Get().GetSession(),
		GeoInfo.nodeId, PartId, &PartInfo), false);

	// Update the volume infos
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVolumeInfo(
		FHoudiniEngine::Get().GetSession(),
		VolumeNodeId, PartInfo.id, &VolumeInfo), false);

	// Volume name
	std::string NameStr;
	FHoudiniEngineUtils::ConvertUnrealString(HeightfieldName, NameStr);

	// Set the Heighfield data on the volume
	float * HeightData = FloatValues.GetData();
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetHeightFieldData(
		FHoudiniEngine::Get().GetSession(),
		GeoInfo.nodeId, PartInfo.id, NameStr.c_str(), HeightData, 0, FloatValues.Num()), false);

	return true;
}

bool FUnrealLandscapeTranslator::AddLandscapeMaterialAttributesToVolume(
	const HAPI_NodeId& VolumeNodeId, 
	const HAPI_PartId& PartId,
	UMaterialInterface* InLandscapeMaterial,
	UMaterialInterface* InLandscapeHoleMaterial,
	UPhysicalMaterial* InPhysicalMaterial)
{
	if (VolumeNodeId == -1)
		return false;

	// LANDSCAPE MATERIAL
	if (InLandscapeMaterial && !InLandscapeMaterial->IsPendingKill())
	{
		// Extract the path name from the material interface
		FString InLandscapeMaterialString = InLandscapeMaterial->GetPathName();

		// Get name of attribute used for marshalling materials.
		std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL;

		// Marshall in material names.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
		//FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoMaterial);
		AttributeInfoMaterial.count = 1;
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		HAPI_Result Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
			MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial);

		if (HAPI_RESULT_SUCCESS == Result)
		{
			// Convert the FString to cont char *
			std::string LandscapeMatCStr = TCHAR_TO_ANSI(*InLandscapeMaterialString);
			const char* LandscapeMatCStrRaw = LandscapeMatCStr.c_str();
			TArray<const char *> LandscapeMatArr;
			LandscapeMatArr.Add(LandscapeMatCStrRaw);

			// Set the attribute's string data
			Result = FHoudiniApi::SetAttributeStringData(
				FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
				MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial,
				LandscapeMatArr.GetData(), 0, AttributeInfoMaterial.count);
		}

		if (Result != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the attribute
			HOUDINI_LOG_WARNING(
				TEXT("Failed to upload unreal_material attribute for landscape: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}
	}

	// HOLE MATERIAL
	if (InLandscapeHoleMaterial && !InLandscapeHoleMaterial->IsPendingKill())
	{
		// Extract the path name from the material interface
		FString InLandscapeMaterialString = InLandscapeHoleMaterial->GetPathName();

		// Get name of attribute used for marshalling materials.
		std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_MATERIAL_HOLE;

		// Marshall in material names.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
		//FMemory::Memzero< HAPI_AttributeInfo >(AttributeInfoMaterial);
		AttributeInfoMaterial.count = 1;
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		HAPI_Result Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
			MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial);

		if (Result == HAPI_RESULT_SUCCESS)
		{
			// Convert the FString to cont char *
			std::string LandscapeMatCStr = TCHAR_TO_ANSI(*InLandscapeMaterialString);
			const char* LandscapeMatCStrRaw = LandscapeMatCStr.c_str();
			TArray<const char *> LandscapeMatArr;
			LandscapeMatArr.Add(LandscapeMatCStrRaw);

			// Set the attribute's string data
			Result = FHoudiniApi::SetAttributeStringData(
				FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
				MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial,
				LandscapeMatArr.GetData(), 0, AttributeInfoMaterial.count);
		}

		if (Result != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the attribute
			HOUDINI_LOG_WARNING(
				TEXT("Failed to upload unreal_hole_material attribute for landscape: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}
	}

	// PHYSICAL MATERIAL
	if (InPhysicalMaterial && !InPhysicalMaterial->IsPendingKill())
	{
		// Extract the path name from the material interface
		FString InPhysMatlString = InPhysicalMaterial->GetPathName();

		// Get name of attribute used for marshalling materials.
		std::string MarshallingAttributeMaterialName = HAPI_UNREAL_ATTRIB_PHYSICAL_MATERIAL;

		// Marshall in material names.
		HAPI_AttributeInfo AttributeInfoMaterial;
		FHoudiniApi::AttributeInfo_Init(&AttributeInfoMaterial);
		AttributeInfoMaterial.count = 1;
		AttributeInfoMaterial.tupleSize = 1;
		AttributeInfoMaterial.exists = true;
		AttributeInfoMaterial.owner = HAPI_ATTROWNER_PRIM;
		AttributeInfoMaterial.storage = HAPI_STORAGETYPE_STRING;
		AttributeInfoMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

		HAPI_Result Result = FHoudiniApi::AddAttribute(
			FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
			MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial);

		if (Result == HAPI_RESULT_SUCCESS)
		{
			// Convert the FString to cont char *
			std::string LandscapeMatCStr = TCHAR_TO_ANSI(*InPhysMatlString);
			const char* LandscapeMatCStrRaw = LandscapeMatCStr.c_str();
			TArray<const char *> LandscapeMatArr;
			LandscapeMatArr.Add(LandscapeMatCStrRaw);

			// Set the attribute's string data
			Result = FHoudiniApi::SetAttributeStringData(
				FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
				MarshallingAttributeMaterialName.c_str(), &AttributeInfoMaterial,
				LandscapeMatArr.GetData(), 0, AttributeInfoMaterial.count);
		}

		if (Result != HAPI_RESULT_SUCCESS)
		{
			// Failed to create the attribute
			HOUDINI_LOG_WARNING(
				TEXT("Failed to upload unreal_physical_material attribute for landscape: %s"),
				*FHoudiniEngineUtils::GetErrorDescription());
		}
	}

	return true;
}

/*
bool 
FUnrealLandscapeTranslator::AddLevelPathAttributeToVolume(
	const HAPI_NodeId& VolumeNodeId,
	const HAPI_PartId& PartId,
	const FString& LevelPath)
{
	if (VolumeNodeId == -1)
		return false;

	// LANDSCAPE MATERIAL
	if (LevelPath.IsEmpty())
		return false;

	// Get name of attribute used for level path
	std::string MarshallingAttributeLevelPath = HAPI_UNREAL_ATTRIB_LEVEL_PATH;

	// Marshall in level path.
	HAPI_AttributeInfo AttributeInfoLevelPath;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLevelPath);
	AttributeInfoLevelPath.count = 1;
	AttributeInfoLevelPath.tupleSize = 1;
	AttributeInfoLevelPath.exists = true;
	AttributeInfoLevelPath.owner = HAPI_ATTROWNER_PRIM;
	AttributeInfoLevelPath.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoLevelPath.originalOwner = HAPI_ATTROWNER_INVALID;

	HAPI_Result Result = FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), VolumeNodeId, PartId,
		MarshallingAttributeLevelPath.c_str(), &AttributeInfoLevelPath);

	if (HAPI_RESULT_SUCCESS == Result)
	{
		// Convert the FString to cont char *
		std::string LevelPathCStr = TCHAR_TO_ANSI(*LevelPath);
		const char* LevelPathCStrRaw = LevelPathCStr.c_str();
		TArray<const char *> LevelPathArr;
		LevelPathArr.Add(LevelPathCStrRaw);

		// Set the attribute's string data
		Result = FHoudiniApi::SetAttributeStringData(
			FHoudiniEngine::Get().GetSession(),
			VolumeNodeId, PartId,
			MarshallingAttributeLevelPath.c_str(), &AttributeInfoLevelPath,
			LevelPathArr.GetData(), 0, AttributeInfoLevelPath.count);
	}

	if (Result != HAPI_RESULT_SUCCESS)
	{
		// Failed to create the attribute
		HOUDINI_LOG_WARNING(
			TEXT("Failed to upload unreal_level_path attribute for landscape: %s"),
			*FHoudiniEngineUtils::GetErrorDescription());
	}

	return true;
}
*/

bool
FUnrealLandscapeTranslator::GetLandscapeLayerData(
	ULandscapeInfo* LandscapeInfo, const int32& LayerIndex,
	TArray<uint8>& LayerData, FLinearColor& LayerUsageDebugColor,
	FString& LayerName)
{
	if (!LandscapeInfo)
		return false;

	// Get the landscape X/Y Size
	int32 MinX = MAX_int32;
	int32 MinY = MAX_int32;
	int32 MaxX = -MAX_int32;
	int32 MaxY = -MAX_int32;
	if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
		return false;

	if (!GetLandscapeLayerData(
		LandscapeInfo, LayerIndex,
		MinX, MinY, MaxX, MaxY,
		LayerData, LayerUsageDebugColor, LayerName))
		return false;

	return true;
}

bool
FUnrealLandscapeTranslator::GetLandscapeLayerData(
	ULandscapeInfo* LandscapeInfo,
	const int32& LayerIndex,
	const int32& MinX, const int32& MinY,
	const int32& MaxX, const int32& MaxY,
	TArray<uint8>& LayerData,
	FLinearColor& LayerUsageDebugColor,
	FString& LayerName)
{
	if (!LandscapeInfo)
		return false;

	if (!LandscapeInfo->Layers.IsValidIndex(LayerIndex))
		return false;

	FLandscapeInfoLayerSettings LayersSetting = LandscapeInfo->Layers[LayerIndex];
	ULandscapeLayerInfoObject* LayerInfo = LayersSetting.LayerInfoObj;
	if (!LayerInfo)
		return false;

	// Calc the X/Y size in points
	int32 XSize = (MaxX - MinX + 1);
	int32 YSize = (MaxY - MinY + 1);
	if ((XSize < 2) || (YSize < 2))
		return false;

	// extracting the uint8 values from the layer
	FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
	LayerData.AddZeroed(XSize * YSize);
	LandscapeEdit.GetWeightDataFast(LayerInfo, MinX, MinY, MaxX, MaxY, LayerData.GetData(), 0);

	LayerUsageDebugColor = LayerInfo->LayerUsageDebugColor;

	LayerName = LayersSetting.GetLayerName().ToString();

	return true;
}

bool
FUnrealLandscapeTranslator::InitDefaultHeightfieldMask(
	const HAPI_VolumeInfo& HeightVolumeInfo,
	const HAPI_NodeId& MaskVolumeNodeId)
{
	// We need to have a mask layer as it is required for proper heightfield functionalities

	// Creating an array filled with 0.0
	TArray< float > MaskFloatData;
	MaskFloatData.Init(0.0f, HeightVolumeInfo.xLength * HeightVolumeInfo.yLength);

	// Creating the volume infos
	HAPI_VolumeInfo MaskVolumeInfo = HeightVolumeInfo;

	// Set the heighfield data in Houdini
	FString MaskName = TEXT("mask");
	HAPI_PartId PartId = 0;
	if (!SetHeightfieldData(MaskVolumeNodeId, PartId, MaskFloatData, MaskVolumeInfo, MaskName))
		return false;

	return true;
}

bool
FUnrealLandscapeTranslator::DestroyLandscapeAssetNode(HAPI_NodeId& ConnectedAssetId, TArray<HAPI_NodeId>& CreatedInputAssetIds)
{
	HAPI_AssetInfo NodeAssetInfo;
	FHoudiniApi::AssetInfo_Init(&NodeAssetInfo);
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetAssetInfo(
		FHoudiniEngine::Get().GetSession(), ConnectedAssetId, &NodeAssetInfo), false);

	FHoudiniEngineString AssetOpName(NodeAssetInfo.fullOpNameSH);
	FString OpName;
	if (!AssetOpName.ToFString(OpName))
		return false;

	if (!OpName.Contains(TEXT("xform")))
	{
		// Not a transform node, so not a Heightfield
		// We just need to destroy the landscape asset node
		return FHoudiniEngineUtils::DestroyHoudiniAsset(ConnectedAssetId);
	}

	// The landscape was marshalled as a heightfield, so we need to destroy and disconnect
	// the volvis nodes, all the merge node's input (each merge input is a volume for one 
	// of the layer/mask of the landscape )

	// Query the volvis node id
	// The volvis node is the fist input of the xform node
	HAPI_NodeId VolvisNodeId = -1;
	FHoudiniApi::QueryNodeInput(
		FHoudiniEngine::Get().GetSession(),
		ConnectedAssetId, 0, &VolvisNodeId);

	// First, destroy the merge node and its inputs
	// The merge node is in the first input of the volvis node
	HAPI_NodeId MergeNodeId = -1;
	FHoudiniApi::QueryNodeInput(
		FHoudiniEngine::Get().GetSession(),
		VolvisNodeId, 0, &MergeNodeId);

	if (MergeNodeId != -1)
	{
		// Get the merge node info
		HAPI_NodeInfo NodeInfo;
		FHoudiniApi::NodeInfo_Init(&NodeInfo);
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetNodeInfo(
			FHoudiniEngine::Get().GetSession(), MergeNodeId, &NodeInfo), false);

		for (int32 n = 0; n < NodeInfo.inputCount; n++)
		{
			// Get the Input node ID from the host ID
			HAPI_NodeId InputNodeId = -1;
			if (HAPI_RESULT_SUCCESS != FHoudiniApi::QueryNodeInput(
				FHoudiniEngine::Get().GetSession(),
				MergeNodeId, n, &InputNodeId))
				break;

			if (InputNodeId == -1)
				break;

			// Disconnect and Destroy that input
			FHoudiniEngineUtils::HapiDisconnectAsset(MergeNodeId, n);
			FHoudiniEngineUtils::DestroyHoudiniAsset(InputNodeId);
		}
	}

	// Second step, destroy all the volumes GEO assets
	for (HAPI_NodeId AssetNodeId : CreatedInputAssetIds)
	{
		FHoudiniEngineUtils::DestroyHoudiniAsset(AssetNodeId);
	}
	CreatedInputAssetIds.Empty();

	// Finally disconnect and destroy the xform, volvis and merge nodes, then destroy them
	FHoudiniEngineUtils::HapiDisconnectAsset(ConnectedAssetId, 0);
	FHoudiniEngineUtils::HapiDisconnectAsset(VolvisNodeId, 0);
	FHoudiniEngineUtils::DestroyHoudiniAsset(MergeNodeId);
	FHoudiniEngineUtils::DestroyHoudiniAsset(VolvisNodeId);

	return FHoudiniEngineUtils::DestroyHoudiniAsset(ConnectedAssetId);
}


bool
FUnrealLandscapeTranslator::ExtractLandscapeData(
	ALandscapeProxy * LandscapeProxy, TSet<ULandscapeComponent *>& SelectedComponents,
	const bool& bExportLighting, const bool& bExportTileUVs, const bool& bExportNormalizedUVs,
	TArray<FVector>& LandscapePositionArray,
	TArray<FVector>& LandscapeNormalArray,
	TArray<FVector>& LandscapeUVArray,
	TArray<FIntPoint>& LandscapeComponentVertexIndicesArray,
	TArray<const char *>& LandscapeComponentNameArray,
	TArray<FLinearColor>& LandscapeLightmapValues)
{
	if (!LandscapeProxy)
		return false;

	if (SelectedComponents.Num() < 1)
		return false;

	// Get runtime settings.
	const UHoudiniRuntimeSettings * HoudiniRuntimeSettings = GetDefault< UHoudiniRuntimeSettings >();

	// Calc all the needed sizes
	int32 ComponentSizeQuads = ((LandscapeProxy->ComponentSizeQuads + 1) >> LandscapeProxy->ExportLOD) - 1;
	float ScaleFactor = (float)LandscapeProxy->ComponentSizeQuads / (float)ComponentSizeQuads;

	int32 NumComponents = SelectedComponents.Num();
	bool bExportOnlySelected = NumComponents != LandscapeProxy->LandscapeComponents.Num();

	int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);
	int32 VertexCount = NumComponents * VertexCountPerComponent;
	if (!VertexCount)
		return false;

	// Initialize the data arrays    
	LandscapePositionArray.SetNumUninitialized(VertexCount);
	LandscapeNormalArray.SetNumUninitialized(VertexCount);
	LandscapeUVArray.SetNumUninitialized(VertexCount);
	LandscapeComponentNameArray.SetNumUninitialized(VertexCount);
	LandscapeComponentVertexIndicesArray.SetNumUninitialized(VertexCount);
	if (bExportLighting)
		LandscapeLightmapValues.SetNumUninitialized(VertexCount);

	//-----------------------------------------------------------------------------------------------------------------
	// EXTRACT THE LANDSCAPE DATA
	//-----------------------------------------------------------------------------------------------------------------
	FIntPoint IntPointMax = FIntPoint::ZeroValue;

	int32 AllPositionsIdx = 0;
	for (int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++)
	{
		ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ComponentIdx];
		if (bExportOnlySelected && !SelectedComponents.Contains(LandscapeComponent))
			continue;

		TArray64< uint8 > LightmapMipData;
		int32 LightmapMipSizeX = 0;
		int32 LightmapMipSizeY = 0;

		// See if we need to export lighting information.
		if (bExportLighting)
		{
			const FMeshMapBuildData* MapBuildData = LandscapeComponent->GetMeshMapBuildData();
			FLightMap2D* LightMap2D = MapBuildData && MapBuildData->LightMap ? MapBuildData->LightMap->GetLightMap2D() : nullptr;
			if (LightMap2D && LightMap2D->IsValid(0))
			{
				UTexture2D * TextureLightmap = LightMap2D->GetTexture(0);
				if (TextureLightmap)
				{
					if (TextureLightmap->Source.GetMipData(LightmapMipData, 0, 0, 0, nullptr))
					{
						LightmapMipSizeX = TextureLightmap->Source.GetSizeX();
						LightmapMipSizeY = TextureLightmap->Source.GetSizeY();
					}
					else
					{
						LightmapMipData.Empty();
					}
				}
			}
		}

		// Construct landscape component data interface to access raw data.
		FLandscapeComponentDataInterface CDI(LandscapeComponent, LandscapeProxy->ExportLOD);

		// Get name of this landscape component.
		const char * LandscapeComponentNameStr = FHoudiniEngineUtils::ExtractRawString(LandscapeComponent->GetName());
		for (int32 VertexIdx = 0; VertexIdx < VertexCountPerComponent; VertexIdx++)
		{
			int32 VertX = 0;
			int32 VertY = 0;
			CDI.VertexIndexToXY(VertexIdx, VertX, VertY);

			// Get position.
			FVector PositionVector = CDI.GetWorldVertex(VertX, VertY);

			// Get normal / tangent / binormal.
			FVector Normal = FVector::ZeroVector;
			FVector TangentX = FVector::ZeroVector;
			FVector TangentY = FVector::ZeroVector;
			CDI.GetLocalTangentVectors(VertX, VertY, TangentX, TangentY, Normal);

			// Export UVs.
			FVector TextureUV = FVector::ZeroVector;
			if (bExportTileUVs)
			{
				// We want to export uvs per tile.
				TextureUV = FVector(VertX, VertY, 0.0f);

				// If we need to normalize UV space.
				if (bExportNormalizedUVs)
					TextureUV /= ComponentSizeQuads;
			}
			else
			{
				// We want to export global uvs (default).
				FIntPoint IntPoint = LandscapeComponent->GetSectionBase();
				TextureUV = FVector(VertX * ScaleFactor + IntPoint.X, VertY * ScaleFactor + IntPoint.Y, 0.0f);

				// Keep track of max offset.
				IntPointMax = IntPointMax.ComponentMax(IntPoint);
			}

			if (bExportLighting)
			{
				FLinearColor VertexLightmapColor(0.0f, 0.0f, 0.0f, 1.0f);
				if (LightmapMipData.Num() > 0)
				{
					FVector2D UVCoord(VertX, VertY);
					UVCoord /= (ComponentSizeQuads + 1);

					FColor LightmapColorRaw = PickVertexColorFromTextureMip(
						LightmapMipData.GetData(), UVCoord, LightmapMipSizeX, LightmapMipSizeY);

					VertexLightmapColor = LightmapColorRaw.ReinterpretAsLinear();
				}

				LandscapeLightmapValues[AllPositionsIdx] = VertexLightmapColor;
			}

			// Retrieve component transform.
			const FTransform & ComponentTransform = LandscapeComponent->GetComponentTransform();

			// Retrieve component scale.
			const FVector & ScaleVector = ComponentTransform.GetScale3D();

			// Perform normalization.
			Normal /= ScaleVector;
			Normal.Normalize();

			TangentX /= ScaleVector;
			TangentX.Normalize();

			TangentY /= ScaleVector;
			TangentY.Normalize();

			// Perform position scaling.
			FVector PositionTransformed = PositionVector / HAPI_UNREAL_SCALE_FACTOR_POSITION;
			LandscapePositionArray[AllPositionsIdx].X = PositionTransformed.X;
			LandscapePositionArray[AllPositionsIdx].Y = PositionTransformed.Z;
			LandscapePositionArray[AllPositionsIdx].Z = PositionTransformed.Y;

			Swap(Normal.Y, Normal.Z);

			// Store landscape component name for this point.
			LandscapeComponentNameArray[AllPositionsIdx] = LandscapeComponentNameStr;

			// Store vertex index (x,y) for this point.
			LandscapeComponentVertexIndicesArray[AllPositionsIdx].X = VertX;
			LandscapeComponentVertexIndicesArray[AllPositionsIdx].Y = VertY;

			// Store point normal.
			LandscapeNormalArray[AllPositionsIdx] = Normal;

			// Store uv.
			LandscapeUVArray[AllPositionsIdx] = TextureUV;

			AllPositionsIdx++;
		}

		// Free the memory allocated for LandscapeComponentNameStr
		FHoudiniEngineUtils::FreeRawStringMemory(LandscapeComponentNameStr);
	}

	// If we need to normalize UV space and we are doing global UVs.
	if (!bExportTileUVs && bExportNormalizedUVs)
	{
		IntPointMax += FIntPoint(ComponentSizeQuads, ComponentSizeQuads);
		IntPointMax = IntPointMax.ComponentMax(FIntPoint(1, 1));

		for (int32 UVIdx = 0; UVIdx < VertexCount; ++UVIdx)
		{
			FVector & PositionUV = LandscapeUVArray[UVIdx];
			PositionUV.X /= IntPointMax.X;
			PositionUV.Y /= IntPointMax.Y;
		}
	}

	return true;
}

FColor
FUnrealLandscapeTranslator::PickVertexColorFromTextureMip(
	const uint8 * MipBytes, FVector2D & UVCoord, int32 MipWidth, int32 MipHeight)
{
	check(MipBytes);

	FColor ResultColor(0, 0, 0, 255);

	if (UVCoord.X >= 0.0f && UVCoord.X < 1.0f && UVCoord.Y >= 0.0f && UVCoord.Y < 1.0f)
	{
		const int32 X = MipWidth * UVCoord.X;
		const int32 Y = MipHeight * UVCoord.Y;

		const int32 Index = ((Y * MipWidth) + X) * 4;

		ResultColor.B = MipBytes[Index + 0];
		ResultColor.G = MipBytes[Index + 1];
		ResultColor.R = MipBytes[Index + 2];
		ResultColor.A = MipBytes[Index + 3];
	}

	return ResultColor;
}

bool 
FUnrealLandscapeTranslator::AddLandscapePositionAttribute(const HAPI_NodeId& NodeId, const TArray< FVector >& LandscapePositionArray)
{
	int32 VertexCount = LandscapePositionArray.Num();
	if (VertexCount < 3)
		return false;

	// Create point attribute info containing positions.    
	HAPI_AttributeInfo AttributeInfoPointPosition;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointPosition);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointPosition );
	AttributeInfoPointPosition.count = VertexCount;
	AttributeInfoPointPosition.tupleSize = 3;
	AttributeInfoPointPosition.exists = true;
	AttributeInfoPointPosition.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointPosition.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointPosition.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointPosition), false);


	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_POSITION, &AttributeInfoPointPosition,
		(const float *)LandscapePositionArray.GetData(),
		0, AttributeInfoPointPosition.count), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeNormalAttribute(const HAPI_NodeId& NodeId, const TArray<FVector>& LandscapeNormalArray)
{
	int32 VertexCount = LandscapeNormalArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoPointNormal;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointNormal);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointNormal );
	AttributeInfoPointNormal.count = VertexCount;
	AttributeInfoPointNormal.tupleSize = 3;
	AttributeInfoPointNormal.exists = true;
	AttributeInfoPointNormal.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointNormal.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointNormal.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_NORMAL, &AttributeInfoPointNormal,
		(const float *)LandscapeNormalArray.GetData(), 0, VertexCount), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeUVAttribute(const HAPI_NodeId& NodeId, const TArray<FVector>& LandscapeUVArray)
{
	int32 VertexCount = LandscapeUVArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoPointUV;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointUV);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointUV );
	AttributeInfoPointUV.count = VertexCount;
	AttributeInfoPointUV.tupleSize = 3;
	AttributeInfoPointUV.exists = true;
	AttributeInfoPointUV.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointUV.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointUV.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_UV, &AttributeInfoPointUV,
		(const float *)LandscapeUVArray.GetData(), 0, AttributeInfoPointUV.count), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeComponentVertexIndicesAttribute(const HAPI_NodeId& NodeId, const TArray<FIntPoint>& LandscapeComponentVertexIndicesArray)
{
	int32 VertexCount = LandscapeComponentVertexIndicesArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoPointLandscapeComponentVertexIndices;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLandscapeComponentVertexIndices);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentVertexIndices );
	AttributeInfoPointLandscapeComponentVertexIndices.count = VertexCount;
	AttributeInfoPointLandscapeComponentVertexIndices.tupleSize = 2;
	AttributeInfoPointLandscapeComponentVertexIndices.exists = true;
	AttributeInfoPointLandscapeComponentVertexIndices.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointLandscapeComponentVertexIndices.storage = HAPI_STORAGETYPE_INT;
	AttributeInfoPointLandscapeComponentVertexIndices.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
		&AttributeInfoPointLandscapeComponentVertexIndices), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeIntData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_LANDSCAPE_VERTEX_INDEX,
		&AttributeInfoPointLandscapeComponentVertexIndices,
		(const int *)LandscapeComponentVertexIndicesArray.GetData(), 0,
		AttributeInfoPointLandscapeComponentVertexIndices.count), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeComponentNameAttribute(const HAPI_NodeId& NodeId, const TArray<const char *>& LandscapeComponentNameArray)
{
	int32 VertexCount = LandscapeComponentNameArray.Num();
	if (VertexCount < 3)
		return false;

	// Create point attribute containing landscape component name.
	HAPI_AttributeInfo AttributeInfoPointLandscapeComponentNames;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLandscapeComponentNames);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLandscapeComponentNames );
	AttributeInfoPointLandscapeComponentNames.count = VertexCount;
	AttributeInfoPointLandscapeComponentNames.tupleSize = 1;
	AttributeInfoPointLandscapeComponentNames.exists = true;
	AttributeInfoPointLandscapeComponentNames.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointLandscapeComponentNames.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoPointLandscapeComponentNames.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
		&AttributeInfoPointLandscapeComponentNames), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_LANDSCAPE_TILE_NAME,
		&AttributeInfoPointLandscapeComponentNames,
		(const char **)LandscapeComponentNameArray.GetData(),
		0, AttributeInfoPointLandscapeComponentNames.count), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeLightmapColorAttribute(const HAPI_NodeId& NodeId, const TArray<FLinearColor>& LandscapeLightmapValues)
{
	int32 VertexCount = LandscapeLightmapValues.Num();

	HAPI_AttributeInfo AttributeInfoPointLightmapColor;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoPointLightmapColor);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPointLightmapColor );
	AttributeInfoPointLightmapColor.count = VertexCount;
	AttributeInfoPointLightmapColor.tupleSize = 4;
	AttributeInfoPointLightmapColor.exists = true;
	AttributeInfoPointLightmapColor.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoPointLightmapColor.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoPointLightmapColor.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId,
		0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_LIGHTMAP_COLOR, &AttributeInfoPointLightmapColor,
		(const float *)LandscapeLightmapValues.GetData(), 0,
		AttributeInfoPointLightmapColor.count), false);

	return true;
}

bool 
FUnrealLandscapeTranslator::AddLandscapeMeshIndicesAndMaterialsAttribute(
	const HAPI_NodeId& NodeId, const bool& bExportMaterials,
	const int32& ComponentSizeQuads, const int32& QuadCount,
	ALandscapeProxy * LandscapeProxy,
	const TSet< ULandscapeComponent * >& SelectedComponents)
{
	if (!LandscapeProxy)
		return false;

	// Compute number of necessary indices.
	int32 IndexCount = QuadCount * 4;
	if (IndexCount < 0)
		return false;

	int32 VertexCountPerComponent = FMath::Square(ComponentSizeQuads + 1);

	// Array holding indices data.
	TArray<int32> LandscapeIndices;
	LandscapeIndices.SetNumUninitialized(IndexCount);

	// Allocate space for face names.
	// The LandscapeMaterial and HoleMaterial per point
	TArray<const char *> FaceMaterials;
	TArray<const char *> FaceHoleMaterials;
	FaceMaterials.SetNumUninitialized(QuadCount);
	FaceHoleMaterials.SetNumUninitialized(QuadCount);

	int32 VertIdx = 0;
	int32 QuadIdx = 0;

	const char * MaterialRawStr = nullptr;
	const char * MaterialHoleRawStr = nullptr;

	// Lambda for freeing the memory allocated by ExtractRawString and returning
	auto FreeMemoryReturn = [&MaterialRawStr, &MaterialHoleRawStr](const bool& bReturn)
	{
		FHoudiniEngineUtils::FreeRawStringMemory(MaterialRawStr);
		FHoudiniEngineUtils::FreeRawStringMemory(MaterialHoleRawStr);

		return bReturn;
	};

	const int32 QuadComponentCount = ComponentSizeQuads + 1;
	for (int32 ComponentIdx = 0; ComponentIdx < LandscapeProxy->LandscapeComponents.Num(); ComponentIdx++)
	{
		ULandscapeComponent * LandscapeComponent = LandscapeProxy->LandscapeComponents[ComponentIdx];
		if (!SelectedComponents.Contains(LandscapeComponent))
			continue;

		if (bExportMaterials)
		{
			// If component has an override material, we need to get the raw name (if exporting materials).
			if (LandscapeComponent->OverrideMaterial)
			{
				MaterialRawStr = FHoudiniEngineUtils::ExtractRawString(LandscapeComponent->OverrideMaterial->GetName());
			}

			// If component has an override hole material, we need to get the raw name (if exporting materials).
			if (LandscapeComponent->OverrideHoleMaterial)
			{
				MaterialHoleRawStr = FHoudiniEngineUtils::ExtractRawString(LandscapeComponent->OverrideHoleMaterial->GetName());
			}
		}

		int32 BaseVertIndex = ComponentIdx * VertexCountPerComponent;
		for (int32 YIdx = 0; YIdx < ComponentSizeQuads; YIdx++)
		{
			for (int32 XIdx = 0; XIdx < ComponentSizeQuads; XIdx++)
			{
				LandscapeIndices[VertIdx + 0] = BaseVertIndex + (XIdx + 0) + (YIdx + 0) * QuadComponentCount;
				LandscapeIndices[VertIdx + 1] = BaseVertIndex + (XIdx + 1) + (YIdx + 0) * QuadComponentCount;
				LandscapeIndices[VertIdx + 2] = BaseVertIndex + (XIdx + 1) + (YIdx + 1) * QuadComponentCount;
				LandscapeIndices[VertIdx + 3] = BaseVertIndex + (XIdx + 0) + (YIdx + 1) * QuadComponentCount;

				// Store override materials (if exporting materials).
				if (bExportMaterials)
				{
					FaceMaterials[QuadIdx] = MaterialRawStr;
					FaceHoleMaterials[QuadIdx] = MaterialHoleRawStr;
				}

				VertIdx += 4;
				QuadIdx++;
			}
		}
	}

	// We can now set vertex list.
	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetVertexList(
		FHoudiniEngine::Get().GetSession(),
		NodeId,	0, LandscapeIndices.GetData(), 0, LandscapeIndices.Num()),
		FreeMemoryReturn(false));

	// We need to generate array of face counts.
	TArray<int32> LandscapeFaces;
	LandscapeFaces.Init(4, QuadCount);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetFaceCounts(
		FHoudiniEngine::Get().GetSession(), 
		NodeId, 0, LandscapeFaces.GetData(), 0, LandscapeFaces.Num()),
		FreeMemoryReturn(false));

	if (bExportMaterials)
	{
		if (!FaceMaterials.Contains(nullptr))
		{
			// Marshall in override primitive material names.
			HAPI_AttributeInfo AttributeInfoPrimitiveMaterial;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrimitiveMaterial);
			//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterial );
			AttributeInfoPrimitiveMaterial.count = FaceMaterials.Num();
			AttributeInfoPrimitiveMaterial.tupleSize = 1;
			AttributeInfoPrimitiveMaterial.exists = true;
			AttributeInfoPrimitiveMaterial.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfoPrimitiveMaterial.storage = HAPI_STORAGETYPE_STRING;
			AttributeInfoPrimitiveMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoPrimitiveMaterial),
				FreeMemoryReturn(false));

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoPrimitiveMaterial,
				(const char **)FaceMaterials.GetData(), 0, AttributeInfoPrimitiveMaterial.count),
				FreeMemoryReturn(false));
		}

		if (!FaceHoleMaterials.Contains(nullptr))
		{
			// Marshall in override primitive material hole names.
			HAPI_AttributeInfo AttributeInfoPrimitiveMaterialHole;
			FHoudiniApi::AttributeInfo_Init(&AttributeInfoPrimitiveMaterialHole);
			//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoPrimitiveMaterialHole );
			AttributeInfoPrimitiveMaterialHole.count = FaceHoleMaterials.Num();
			AttributeInfoPrimitiveMaterialHole.tupleSize = 1;
			AttributeInfoPrimitiveMaterialHole.exists = true;
			AttributeInfoPrimitiveMaterialHole.owner = HAPI_ATTROWNER_PRIM;
			AttributeInfoPrimitiveMaterialHole.storage = HAPI_STORAGETYPE_STRING;
			AttributeInfoPrimitiveMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
				&AttributeInfoPrimitiveMaterialHole),
				FreeMemoryReturn(false));

			HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
				FHoudiniEngine::Get().GetSession(),
				NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
				&AttributeInfoPrimitiveMaterialHole, (const char **)FaceHoleMaterials.GetData(), 0,
				AttributeInfoPrimitiveMaterialHole.count),
				FreeMemoryReturn(false));
		}
	}		

	// Free the memory and return true
	return FreeMemoryReturn(true);
}

bool 
FUnrealLandscapeTranslator::AddLandscapeGlobalMaterialAttribute(
	const HAPI_NodeId& NodeId, ALandscapeProxy * LandscapeProxy)
{
	if (!LandscapeProxy)
		return false;

	// If there's a global landscape material, we marshall it as detail.
	UMaterialInterface * MaterialInterface = LandscapeProxy->GetLandscapeMaterial();
	const char * MaterialNameStr = "";
	if (MaterialInterface)
	{
		FString FullMaterialName = MaterialInterface->GetPathName();
		MaterialNameStr = TCHAR_TO_UTF8(*FullMaterialName);
	}

	HAPI_AttributeInfo AttributeInfoDetailMaterial;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoDetailMaterial);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterial );
	AttributeInfoDetailMaterial.count = 1;
	AttributeInfoDetailMaterial.tupleSize = 1;
	AttributeInfoDetailMaterial.exists = true;
	AttributeInfoDetailMaterial.owner = HAPI_ATTROWNER_DETAIL;
	AttributeInfoDetailMaterial.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoDetailMaterial.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoDetailMaterial), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
		FHoudiniEngine::Get().GetSession(), NodeId, 0,
		HAPI_UNREAL_ATTRIB_MATERIAL, &AttributeInfoDetailMaterial,
		(const char**)&MaterialNameStr, 0, AttributeInfoDetailMaterial.count), false);

	// If there's a global landscape hole material, we marshall it as detail.
	UMaterialInterface * HoleMaterialInterface = LandscapeProxy->GetLandscapeHoleMaterial();
	const char * HoleMaterialNameStr = "";
	if (HoleMaterialInterface)
	{
		FString FullMaterialName = HoleMaterialInterface->GetPathName();
		MaterialNameStr = TCHAR_TO_UTF8(*FullMaterialName);
	}

	HAPI_AttributeInfo AttributeInfoDetailMaterialHole;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoDetailMaterialHole);
	//FMemory::Memzero< HAPI_AttributeInfo >( AttributeInfoDetailMaterialHole );
	AttributeInfoDetailMaterialHole.count = 1;
	AttributeInfoDetailMaterialHole.tupleSize = 1;
	AttributeInfoDetailMaterialHole.exists = true;
	AttributeInfoDetailMaterialHole.owner = HAPI_ATTROWNER_DETAIL;
	AttributeInfoDetailMaterialHole.storage = HAPI_STORAGETYPE_STRING;
	AttributeInfoDetailMaterialHole.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
		&AttributeInfoDetailMaterialHole), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeStringData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0, HAPI_UNREAL_ATTRIB_MATERIAL_HOLE,
		&AttributeInfoDetailMaterialHole, (const char **)&HoleMaterialNameStr, 0,
		AttributeInfoDetailMaterialHole.count), false);

	return true;
}


bool
FUnrealLandscapeTranslator::AddLandscapeLayerAttribute(
	const HAPI_NodeId& NodeId, const TArray<float>& LandscapeLayerArray, const FString& LayerName)
{
	int32 VertexCount = LandscapeLayerArray.Num();
	if (VertexCount < 3)
		return false;

	HAPI_AttributeInfo AttributeInfoLayer;
	FHoudiniApi::AttributeInfo_Init(&AttributeInfoLayer);
	AttributeInfoLayer.count = VertexCount;
	AttributeInfoLayer.tupleSize = 1;
	AttributeInfoLayer.exists = true;
	AttributeInfoLayer.owner = HAPI_ATTROWNER_POINT;
	AttributeInfoLayer.storage = HAPI_STORAGETYPE_FLOAT;
	AttributeInfoLayer.originalOwner = HAPI_ATTROWNER_INVALID;

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::AddAttribute(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0,
		TCHAR_TO_ANSI(*LayerName),
		&AttributeInfoLayer), false);

	HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::SetAttributeFloatData(
		FHoudiniEngine::Get().GetSession(),
		NodeId, 0,
		TCHAR_TO_ANSI(*LayerName),
		&AttributeInfoLayer,
		(const float *)LandscapeLayerArray.GetData(), 
		0, AttributeInfoLayer.count), false);

	return true;
}