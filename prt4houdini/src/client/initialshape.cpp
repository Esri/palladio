#include "initialshape.h"

#include "GU/GU_PrimPoly.h"
#include "OP/OP_Operator.h"
#include "OP/OP_OperatorTable.h"
#include "OP/OP_Director.h"

#include "boost/foreach.hpp"
#include "boost/dynamic_bitset.hpp"
#include "boost/variant.hpp"


namespace p4h {

InitialShapeGenerator::InitialShapeGenerator(
		GU_Detail* gdp,
		InitialShapeContext& isCtx
) : mISB(prt::InitialShapeBuilder::create()) {
	createInitialShapes(gdp, isCtx);
}

InitialShapeGenerator::~InitialShapeGenerator() {
	BOOST_FOREACH(const prt::InitialShape* is, mInitialShapes) {
					is->destroy();
				}
	BOOST_FOREACH(const prt::AttributeMap* am, mInitialShapeAttributes) {
					am->destroy();
				}
}

namespace {

const bool PDBG = false;

struct PrimitivePartition {
	typedef boost::variant<UT_String, fpreal64, int64> ClassifierValueType;
	typedef std::vector<GA_Primitive*> PrimitiveVector;
	typedef std::map<ClassifierValueType, PrimitiveVector> PartitionMap;

	PrimitivePartition(GA_ROAttributeRef clsAttr, GA_StorageClass clsType)
			: mClsAttr(clsAttr), mClsType(clsType) {
		if (PDBG)
			LOG_DBG << "   -- prim part: partition attr = " << (mClsAttr.isValid() ? mClsAttr->getName() : "<invalid>");
	}

	void add(GA_Detail* gdp, GA_Primitive* p) {
		if (PDBG) LOG_DBG << "      adding prim: " << gdp->primitiveIndex(p->getMapOffset());

		// if classification attr is missing, we exit early
		if (mClsAttr.isInvalid()) {
			if (PDBG) LOG_DBG << "       missing cls attr!";
			return;
		}

		if ((mClsType == GA_STORECLASS_FLOAT) && mClsAttr.isFloat()) {
			GA_ROHandleD av(gdp, GA_ATTRIB_PRIMITIVE, mClsAttr->getName());
			if (av.isValid()) {
				fpreal64 v = av.get(p->getMapOffset());
				if (PDBG) LOG_DBG << "        got float classifier value: " << v;
				mPrimitives[v].push_back(p);
			}
			else {
				if (PDBG) LOG_DBG << "        float: invalid handle!";
			}
		}
		else if ((mClsType == GA_STORECLASS_INT) && mClsAttr.isInt()) {
			GA_ROHandleID av(gdp, GA_ATTRIB_PRIMITIVE, mClsAttr->getName());
			if (av.isValid()) {
				int64 v = av.get(p->getMapOffset());
				LOG_DBG << "        got int classifier value: " << v;
				mPrimitives[v].push_back(p);
			}
			else {
				if (PDBG) LOG_DBG << "        int: invalid handle!";
			}
		}
		else if ((mClsType == GA_STORECLASS_STRING) && mClsAttr.isString()) {
			GA_ROHandleS av(gdp, GA_ATTRIB_PRIMITIVE, mClsAttr->getName());
			if (av.isValid()) {
				const char* v = av.get(p->getMapOffset());
				assert(v != nullptr);
				LOG_DBG << "        got string classifier value: " << v;
				mPrimitives[UT_String(v)].push_back(p);
			}
			else {
				if (PDBG) LOG_DBG << "        string: invalid handle!";
			}
		}
		else {
			if (PDBG) LOG_DBG << "        wrong type!";
		}
	}

	const PartitionMap& get() const {
		return mPrimitives;
	}

	GA_ROAttributeRef mClsAttr;
	GA_StorageClass mClsType;
	PartitionMap mPrimitives;
};

} // namespace

void InitialShapeGenerator::createInitialShapes(
		GU_Detail* gdp,
		InitialShapeContext& isCtx
) {
	static const bool DBG = true;
	if (DBG) LOG_DBG << "-- createInitialShapes";

	// try to find primitive partitioning attribute
	GA_ROAttributeRef shapeClsAttrRef;
	GA_ROAttributeRef r(gdp->findPrimitiveAttribute(isCtx.mShapeClsAttrName.buffer()));
	if (r->getStorageClass() == isCtx.mShapeClsType)
		shapeClsAttrRef = r;

	// partition primitives by shapeClsAttrRef value
	// in case shapeClsAttrRef is invalid, all prims end up in the same initial shape
	PrimitivePartition primPart(shapeClsAttrRef, isCtx.mShapeClsType);
	GA_Primitive* prim = nullptr;
	GA_FOR_ALL_PRIMITIVES(gdp, prim) {
		primPart.add(gdp, prim);
	}

	// reuse same vertex list for all initial shape (prt throws rest away)
	std::vector<double> vtx;
	GA_Offset ptoff;
	GA_FOR_ALL_PTOFF(gdp, ptoff) {
		UT_Vector3 p = gdp->getPos3(ptoff);
		vtx.push_back(static_cast<double>(p.x()));
		vtx.push_back(static_cast<double>(p.y()));
		vtx.push_back(static_cast<double>(p.z()));
	}

	// create initial shape for each primitive partition
	size_t userAttrCount = 0;
	const wchar_t* const* cUserAttrKeys = isCtx.mUserAttributeValues->getKeys(&userAttrCount);
	const PrimitivePartition::PartitionMap& pm = primPart.get();
	size_t isIdx = 0;
	for (auto pIt = pm.begin(); pIt != pm.end(); ++pIt, ++isIdx) {
		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pIt->second.size();
		std::vector<uint32_t> idx, faceCounts;
		boost::dynamic_bitset<> setAttributes(userAttrCount);

		// copy global attribute source builder for each initial shape
		AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::createFromAttributeMap(isCtx.mUserAttributeValues.get()));

		// loop over all primitives inside partition
		for (auto p: pIt->second) {
			// get face vertex indices & counts
			GU_PrimPoly* face = static_cast<GU_PrimPoly*>(p);
			for (GA_Size i = face->getVertexCount() - 1; i >= 0; i--) {
				GA_Offset off = face->getPointOffset(i);
				idx.push_back(static_cast<uint32_t>(off));
			}
			faceCounts.push_back(static_cast<uint32_t>(face->getVertexCount()));

			// extract primitive attributes
			// if multi-prim initial shape: the first encountered value per attribute wins
			for (size_t k = 0; k < userAttrCount; k++) {
				if (!setAttributes[k]) {
					const wchar_t* key = cUserAttrKeys[k];
					std::string nKey = utils::toOSNarrowFromUTF16(key);
					std::string nLabel = nKey.substr(nKey.find_first_of('$') + 1); // strip style

					GA_ROAttributeRef ar(gdp->findPrimitiveAttribute(nLabel.c_str()));
					if (ar.isInvalid())
						continue;

					switch (isCtx.mUserAttributeValues->getType(key)) {
						case prt::AttributeMap::PT_FLOAT: {
							double userAttrValue = isCtx.mUserAttributeValues->getFloat(key);
							double ruleAttrValue = isCtx.mRuleAttributeValues->getFloat(key);
							if (std::abs(userAttrValue - ruleAttrValue) < 1e-5) {
								GA_ROHandleD av(gdp, GA_ATTRIB_PRIMITIVE, ar->getName());
								if (av.isValid()) {
									double v = av.get(p->getMapOffset());
									if (DBG) LOG_DBG << "   prim float attr: " << ar->getName() << " = " << v;
									amb->setFloat(key, v);
									setAttributes.set(k);
								}
							}
							break;
						}
						case prt::AttributeMap::PT_STRING: {
							const wchar_t* userAttrValue = isCtx.mUserAttributeValues->getString(key);
							const wchar_t* ruleAttrValue = isCtx.mRuleAttributeValues->getString(key);
							if (std::wcscmp(userAttrValue, ruleAttrValue) == 0) {
								GA_ROHandleS av(gdp, GA_ATTRIB_PRIMITIVE, ar->getName());
								if (av.isValid()) {
									const char* v = av.get(p->getMapOffset());
									if (DBG) LOG_DBG << "   prim string attr: " << ar->getName() << " = " << v;
									std::wstring wv = utils::toUTF16FromOSNarrow(v);
									amb->setString(key, wv.c_str());
									setAttributes.set(k);
								}
							}
							break;
						}
						case prt::AttributeMap::PT_BOOL: {
							bool userAttrValue = isCtx.mUserAttributeValues->getBool(key);
							bool ruleAttrValue = isCtx.mRuleAttributeValues->getBool(key);
							if (userAttrValue != ruleAttrValue) {
								GA_ROHandleI av(gdp, GA_ATTRIB_PRIMITIVE, ar->getName());
								if (av.isValid()) {
									int v = av.get(p->getMapOffset());
									if (DBG) LOG_DBG << "   prim bool attr: " << ar->getName() << " = " << v;
									amb->setBool(key, (v > 0));
									setAttributes.set(k);
								}
							}
							break;
						}
						default: {
							LOG_WRN << "attribute " << nKey << ": type not handled";
							break;
						}
					} // switch key type
				} // setAttributes?
			} // for each user attr
		} // for each prim

		mISB->setGeometry(vtx.data(), vtx.size(), idx.data(), idx.size(), faceCounts.data(), faceCounts.size());

		const prt::AttributeMap* initialShapeAttrs = amb->createAttributeMap();
		mInitialShapeAttributes.push_back(initialShapeAttrs);

		std::wstring shapeName = L"shape_" + std::to_wstring(isIdx);
		std::wstring startRule = isCtx.mStyle + L"$" + isCtx.mStartRule;
		mISB->setAttributes(
				isCtx.mRuleFile.c_str(),
				startRule.c_str(),
				isCtx.mSeed,
				shapeName.c_str(),
				initialShapeAttrs,
				isCtx.mAssetsMap.get()
		);

		prt::Status status = prt::STATUS_UNSPECIFIED_ERROR;
		const prt::InitialShape* initialShape = mISB->createInitialShapeAndReset(&status);
		if (status != prt::STATUS_OK) {
			LOG_WRN << "failed to create initial shape: " << prt::getStatusDescription(status);
			initialShapeAttrs->destroy();
			return;
		}
		mInitialShapes.push_back(initialShape);

		if (DBG) LOG_DBG << p4h::utils::objectToXML(initialShape);
	} // for each parition
}

} // namespace p4h
