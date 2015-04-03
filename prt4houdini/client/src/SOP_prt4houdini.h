/*
 * Copyright (c) 2014
 *	Side Effects Software Inc.  All rights reserved.
 *
 * Redistribution and use of Houdini Development Kit samples in source and
 * binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. The name of Side Effects Software may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *----------------------------------------------------------------------------
 * PointWave SOP
 */


#ifndef __SOP_PRT4HOUDINI__
#define __SOP_PRT4HOUDINI__

#include "SOP/SOP_Node.h"


namespace prt4hdn {


class SOP_PRT : public SOP_Node {
public:
	static PRM_Template	myTemplateList[];
	static OP_Node* myConstructor(OP_Network*, const char*, OP_Operator*);

	SOP_PRT(OP_Network *net, const char *name, OP_Operator *op);
	virtual ~SOP_PRT();
	virtual OP_ERROR cookInputGroups(OP_Context &context, int alone = 0);

protected:
	virtual OP_ERROR	 cookMySop(OP_Context &context);
	// 	virtual const char*	inputLabel(unsigned) const;

private:
	void	getGroups(UT_String &str){ evalString(str, "group", 0, 0); }

	GU_DetailGroupPair			myDetailGroupPair;
	const GA_PointGroup*		mPointGroup;
	const GA_PrimitiveGroup*	mPrimitiveGroup;
};


} // namespace prt4hdn

#endif // __SOP_PRT4HOUDINI__
