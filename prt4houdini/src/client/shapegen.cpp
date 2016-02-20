#include "client/shapegen.h"

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
			: mClsAttr(clsAttr), mClsType(clsType)
	{
		if (PDBG) LOG_DBG << "   -- prim part: partition attr = " << (mClsAttr.isValid() ? mClsAttr->getName() : "<invalid>");
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

	// collect all primitive attributes
	if (DBG) LOG_DBG << "   -- all primitive attributes on current detail:";
	GA_ROAttributeRef shapeClsAttrRef;
	std::vector<GA_ROAttributeRef> primitiveAttributes;
	GA_Attribute* gaAttr = nullptr;
	GA_FOR_ALL_PRIMITIVE_ATTRIBUTES(gdp, gaAttr) {
		primitiveAttributes.push_back(GA_ROAttributeRef(gaAttr));
		GA_ROAttributeRef& r = primitiveAttributes.back();
		if (DBG) LOG_DBG << "      prim attr: " << r->getName();

		// try to detect the shape classification attribute for partitioning below
		if (isCtx.mShapeClsAttrName.equal(r->getName(), true)) {
			if (DBG) LOG_DBG << "         shape cls name match, storage class = " << r->getStorageClass();
			if (r->getStorageClass() == isCtx.mShapeClsType)
				shapeClsAttrRef = r;
		}
	}

	// partition primitives by delimAttr value
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
	size_t isIdx = 0;
	const PrimitivePartition::PartitionMap& pm = primPart.get();
	for (auto pIt = pm.begin(); pIt != pm.end(); ++pIt, ++isIdx) {
		if (DBG) LOG_DBG << "   -- creating initial shape " << isIdx << ", prim count = " << pIt->second.size();
		std::vector<uint32_t> idx, faceCounts;
		boost::dynamic_bitset<> setAttributes(primitiveAttributes.size());

		// copy global attribute source builder for each initial shape
		AttributeMapPtr amt(isCtx.mAttributeSource->createAttributeMap());
		AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::createFromAttributeMap(amt.get()));

		for (auto p: pIt->second) {
			// get face vertex indices & counts
			GU_PrimPoly* face = static_cast<GU_PrimPoly*>(p);
			for (GA_Size i = face->getVertexCount() - 1; i >= 0; i--) {
				GA_Offset off = face->getPointOffset(i);
				idx.push_back(static_cast<uint32_t>(off));
			}
			faceCounts.push_back(static_cast<uint32_t>(face->getVertexCount()));

			// extract primitive attributes
			// if multi-prim initial shape: the first encounterd value per attribute wins
			// TODO: only use prim attrs, if node value != rule value, see ticket #10
			for (size_t ai = 0; ai < primitiveAttributes.size(); ai++) {
				if (!setAttributes[ai]) {
					const GA_ROAttributeRef& ar = primitiveAttributes[ai];
					if (ar.isValid()) {
						if (ar.isFloat()) {
							GA_ROHandleD av(gdp, GA_ATTRIB_PRIMITIVE, ar->getName());
							if (av.isValid()) {
								double v = av.get(p->getMapOffset());
								if (DBG) LOG_DBG << "   setting float attrib " << ar->getName() << " = " << v;
								std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
								amb->setFloat(wn.c_str(), v);
								setAttributes.set(ai);
							}
						}
						else if (ar.isInt()) {
							GA_ROHandleID av(gdp, GA_ATTRIB_PRIMITIVE, ar->getName());
							if (av.isValid()) {
								int64 v = av.get(p->getMapOffset());
								if (DBG) LOG_DBG << "   setting int (!) attrib " << ar->getName() << " = " << v;
								std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
								amb->setFloat(wn.c_str(), v); // !!! no int in CGA !!!
								setAttributes.set(ai);
							}
						} else if (ar.isString()) {
							GA_ROHandleS av(gdp, GA_ATTRIB_PRIMITIVE, ar->getName());
							if (av.isValid()) {
								const char* v = av.get(p->getMapOffset());
								if (DBG) LOG_DBG << "   setting string attrib " << ar->getName() << " = " << v;
								std::wstring wn = utils::toUTF16FromOSNarrow(ar->getName());
								std::wstring wv = utils::toUTF16FromOSNarrow(v);
								amb->setString(wn.c_str(), wv.c_str());
								setAttributes.set(ai);
							}
						}
					}
				}
			}
		} // for all prims in partition

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
	}
}

} // namespace p4h
