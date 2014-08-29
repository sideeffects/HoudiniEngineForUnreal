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
 * Based on http://bloglitb.blogspot.ca/2010/07/access-to-private-members-thats-easy.html
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
	struct Patcher : Result<Tag>
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


#define HOUDINI_PRIVATE_PATCH(PATCH_ACCESSOR, PATCH_METHOD)												\
	template struct PrivatePatch::Patcher<PATCH_ACCESSOR, &PATCH_METHOD>

#define HOUDINI_PRIVATE_CALL_EXT_PARM1(PATCH_ACCESSOR, PATCH_BASECLASS, PATCH_INSTANCE, PATCH_PARAM)	\
	do																									\
	{																									\
		PATCH_BASECLASS& b = *PATCH_INSTANCE;															\
		(b.*PrivatePatch::Result<PATCH_ACCESSOR>::ptr)(PATCH_PARAM);									\
	}																									\
	while(0)

#define HOUDINI_PRIVATE_CALL_EXT_NOPARM(PATCH_ACCESSOR, PATCH_BASECLASS, PATCH_INSTANCE)				\
	do																									\
	{																									\
		PATCH_BASECLASS& b = *PATCH_INSTANCE;															\
		(b.*PrivatePatch::Result<PATCH_ACCESSOR>::ptr)();												\
	}																									\
	while(0)

#define HOUDINI_PRIVATE_CALL_PARM1(PATCH_ACCESSOR, PATCH_BASECLASS, PATCH_PARAM)						\
	HOUDINI_PRIVATE_CALL_EXT_PARM1(PATCH_ACCESSOR, PATCH_BASECLASS, this, PATCH_PARAM)

#define HOUDINI_PRIVATE_CALL_NOPARM(PATCH_ACCESSOR, PATCH_BASECLASS)									\
	HOUDINI_PRIVATE_CALL_EXT_NOPARM(PATCH_ACCESSOR, PATCH_BASECLASS, this)
