/*
* PROPRIETARY INFORMATION.  This software is proprietary to
* Side Effects Software Inc., and is not to be reproduced,
* transmitted, or disclosed in any way without written permission.
*
* Produced by:
*      Mykola Konyk
*      Side Effects Software Inc
*      123 Front Street West, Suite 1401
*      Toronto, Ontario
*      Canada   M5J 2M2
*      416-504-9876
*
*/

#pragma once

class FHoudiniMeshVertexBuffer : public FVertexBuffer
{
public:

	/** Default constructor. **/
	FHoudiniMeshVertexBuffer();

public: /** FRenderResource methods. **/

	virtual void InitRHI() override;

public:

	/** Check if field is used by vertex in this vertex buffer. **/
	bool CheckUsedField(EHoudiniMeshVertexField::Type Field) const;

public:

	/** Array of stored vertices. **/
	TArray<FHoudiniMeshVertex> Vertices;

	/** Descriptor of which fields are used by vertices. **/
	int32 VertexUsedFields;
};
