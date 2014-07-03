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

namespace PrivatePatch
{
	template<typename Tag>
	struct Result
	{
		typedef typename Tag::type type;
		static type ptr;
	};

	template<typename Tag> typename Result<Tag>::type Result<Tag>::ptr;

	template<typename Tag, typename Tag::type p>
	struct Patcher : Result < Tag >
	{
		struct Filler
		{
			Filler()
			{
				Result<Tag>::ptr = p;
			}
		};

		static Filler FillerObj;
	};

	template<typename Tag, typename Tag::type p> typename Patcher<Tag, p>::Filler Patcher<Tag, p>::FillerObj;
}


#define HOUDINI_PRIVATE_PATCH(PATCH_ACCESSOR, PATCH_METHOD)						\
	template struct PrivatePatch::Patcher<PATCH_ACCESSOR, &PATCH_METHOD>
	
#define HOUDINI_PRIVATE_CALL(PATCH_ACCESSOR, PATCH_BASECLASS, PATCH_PARAM)		\
	do																			\
	{																			\
		PATCH_BASECLASS& b = *this;												\
		(b.*PrivatePatch::Result<PATCH_ACCESSOR>::ptr)(PATCH_PARAM);			\
	}																			\
	while(0)
