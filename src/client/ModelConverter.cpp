#include "ModelConverter.h"
#include "ShapeConverter.h"
#include "LogHandler.h"
#include "utils.h"

#include "GU/GU_HoleInfo.h"

#include <chrono>


namespace {

constexpr bool DBG = false;

typedef std::map<std::wstring, GA_RWHandleS> StringHandles; // prt string
typedef std::map<std::wstring, GA_RWHandleI> IntHandles; // prt int32_t -> int32_t
typedef std::map<std::wstring, GA_RWHandleC> BoolHandles; // prt bool -> int8_t
typedef std::map<std::wstring, GA_RWHandleF> FloatHandles; // prt double -> float !!!

struct HandleMaps {
	StringHandles mStrings;
	IntHandles mInts;
	BoolHandles mBools;
	FloatHandles mFloats;
};

void applyAttributeMap(const HandleMaps& hm, const prt::AttributeMap* m, const GA_Offset rangeStart, const GA_Offset rangePastEnd) {
	for (auto& h: hm.mStrings) {
		if (m->hasKey(h.first.c_str())) {
			std::string nv;
			const wchar_t* v = m->getString(h.first.c_str());
			if (v && std::wcslen(v) > 0) {
				nv = toOSNarrowFromUTF16(v);
				for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
					h.second.set(off, nv.c_str());
				if (DBG)
					LOG_DBG << "string attr: range = [" << rangeStart << ", " << rangePastEnd << "): "
					        << h.second.getAttribute()->getName() << " = " << nv;
			}
		}
	}

	for (auto& h: hm.mFloats) {
		if (m->hasKey(h.first.c_str())) {
			const auto v = m->getFloat(h.first.c_str());
			for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
				h.second.set(off, static_cast<fpreal32 >(v));
			if (DBG)
				LOG_DBG << "float attr: range = [" << rangeStart << ", " << rangePastEnd << "): "
				        << h.second.getAttribute()->getName() << " = " << v;
		}
	}

	for (auto& h: hm.mInts) {
		if (m->hasKey(h.first.c_str())) {
			const int32_t v = m->getInt(h.first.c_str());
			for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
				h.second.set(off, v);
			if (DBG)
				LOG_DBG << "float attr: range = [" << rangeStart << ", " << rangePastEnd << "): " <<
				        h.second.getAttribute()->getName() << " = " << v;
		}
	}

	for (auto& h: hm.mBools) {
		if (m->hasKey(h.first.c_str())) {
			const bool v = m->getBool(h.first.c_str());
			constexpr int8_t valFalse = 0;
			constexpr int8_t valTrue = 1;
			for (GA_Offset off = rangeStart; off < rangePastEnd; off++)
				h.second.set(off, v ? valTrue : valFalse);
			if (DBG)
				LOG_DBG << "bool attr: range = [" << rangeStart << ", " << rangePastEnd << "): " <<
				        h.second.getAttribute()->getName() << " = " << v;
		}
	}
}

void setupHandles(GU_Detail* detail, const prt::AttributeMap* m, HandleMaps& hm) {
	size_t keyCount = 0;
	wchar_t const* const* keys = m->getKeys(&keyCount);
	for(size_t k = 0; k < keyCount; k++) {
		wchar_t const* const key = keys[k];
		const std::string nKey = toOSNarrowFromUTF16(key);
		const UT_String utKey = NameConversion::toPrimAttr(nKey);

		if (DBG) LOG_DBG << "nKey = " << nKey;
		switch(m->getType(key)) {
			case prt::Attributable::PT_BOOL: {
				if (hm.mBools.count(key) > 0)
					continue;
				GA_RWHandleC h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, 1, GA_Defaults(0), nullptr, nullptr, GA_STORE_INT8));
				if (h.isValid()) {
					hm.mBools.insert(std::make_pair(key, h));
					if (DBG) LOG_DBG << "bool: " << h->getName();
				}
				else
					LOG_ERR << "could not create primitive attribute " << utKey;
				break;
			}
			case prt::Attributable::PT_FLOAT: {
				if (hm.mFloats.count(key) > 0)
					continue;
				GA_RWHandleF h(detail->addFloatTuple(GA_ATTRIB_PRIMITIVE, utKey, 1));
				if (h.isValid()) {
					hm.mFloats.insert(std::make_pair(key, h));
					if (DBG) LOG_DBG << "float: " << h->getName();
				}
				else
					LOG_ERR << "could not create primitive attribute " << utKey;
				break;
			}
			case prt::Attributable::PT_INT: {
				if (hm.mInts.count(key) > 0)
					continue;
				GA_RWHandleI h(detail->addIntTuple(GA_ATTRIB_PRIMITIVE, utKey, 1));
				if (h.isValid()) {
					hm.mInts.insert(std::make_pair(key, h));
					if (DBG) LOG_DBG << "int: " << h->getName();
				}
				else
					LOG_ERR << "could not create primitive attribute " << utKey;
				break;
			}
			case prt::Attributable::PT_STRING: {
				if (hm.mStrings.count(key) > 0)
					continue;
				GA_RWHandleS h(detail->addStringTuple(GA_ATTRIB_PRIMITIVE, utKey, 1));
				if (h.isValid()) {
					hm.mStrings.insert(std::make_pair(key, h));
					if (DBG) LOG_DBG << "strings: " << h->getName();
				}
				else
					LOG_ERR << "could not create primitive attribute " << utKey;
				break;
			}
//			case prt::Attributable::PT_BOOL_ARRAY: {
//				break;
//			}
//			case prt::Attributable::PT_INT_ARRAY: {
//				break;
//			}
//			case prt::Attributable::PT_FLOAT_ARRAY: {
//				break;
//			}
//			case prt::Attributable::PT_STRING_ARRAY:{
//				break;
//			}
			default:
				if (DBG) LOG_DBG << "ignored: " << key;
				break;
		}
	}
}

std::mutex mDetailMutex; // guard the houdini detail object

using UTVector3FVector = std::vector<UT_Vector3F>;

UTVector3FVector convertVertices(const double* vtx, size_t vtxSize) {
	UTVector3FVector utPoints;
	utPoints.reserve(vtxSize / 3);
	for (size_t pi = 0; pi < vtxSize; pi += 3) {
		const auto x = static_cast<fpreal32>(vtx[pi + 0]);
		const auto y = static_cast<fpreal32>(vtx[pi + 1]);
		const auto z = static_cast<fpreal32>(vtx[pi + 2]);
		utPoints.emplace_back(x, y, z);
	}
	return utPoints;
}

void setVertexNormals(
	GA_RWHandleV3& handle, const GA_Detail::OffsetMarker& marker,
	const double* nrm, size_t nrmSize, const uint32_t* indices
) {
	uint32_t vi = 0;
	for (GA_Iterator it(marker.vertexRange()); !it.atEnd(); ++it, ++vi) {
		const auto nrmIdx = indices[vi];
		const auto nrmPos = nrmIdx*3;
		assert(nrmPos + 2 < nrmSize);
		const auto nx = static_cast<fpreal32>(nrm[nrmPos + 0]);
		const auto ny = static_cast<fpreal32>(nrm[nrmPos + 1]);
		const auto nz = static_cast<fpreal32>(nrm[nrmPos + 2]);
		handle.set(it.getOffset(), UT_Vector3F(nx, ny, nz));
	}
}

constexpr auto UV_IDX_NO_VALUE = uint32_t(-1);

} // namespace


namespace ModelConversion {

void getUVSet(std::vector<uint32_t>& uvIndicesPerSet,
              const uint32_t* counts, size_t countsSize,
              const uint32_t* uvCounts, size_t uvCountsSize,
              uint32_t uvSet, uint32_t uvSets,
              const uint32_t* uvIndices, size_t uvIndicesSize)
{
	assert(uvSet < uvSets);

	uint32_t uvIdxBase = 0;
	for (size_t c = 0; c < countsSize; c++) {
		const uint32_t vtxCnt = counts[c];

		assert(uvSets * c + uvSet < uvCountsSize);
		const uint32_t uvCnt = uvCounts[uvSets * c + uvSet]; // 0 or vtxCnt
		assert(uvCnt == 0 || uvCnt == vtxCnt);
		if (uvCnt == vtxCnt) {

			// skip to desired uv set index range
			uint32_t uvIdxOff = 0;
			for (size_t uvi = 0; uvi < uvSet; uvi++)
				uvIdxOff += uvCounts[c*uvSets + uvi];

			const uint32_t uvIdxPos = uvIdxBase + uvIdxOff;
			for (size_t vi = 0; vi < uvCnt; vi++) {
				assert(uvIdxPos + vi < uvIndicesSize);
				const uint32_t uvIdx = uvIndices[uvIdxPos + vi];
				uvIndicesPerSet.push_back(uvIdx);
			}
		} else
			uvIndicesPerSet.insert(uvIndicesPerSet.end(), vtxCnt, UV_IDX_NO_VALUE);

		// skip to next face
		for (size_t uvSet = 0; uvSet < uvSets; uvSet++)
			uvIdxBase += uvCounts[c*uvSets + uvSet];
	}
}

void setUVs(GA_RWHandleV3& handle, const GA_Detail::OffsetMarker& marker,
            const uint32_t* counts, size_t countsSize,
            const uint32_t* uvCounts, size_t uvCountsSize,
            uint32_t uvSet, uint32_t uvSets,
            const uint32_t* uvIndices, size_t uvIndicesSize,
            const double* uvs, size_t uvsSize)
{
	std::vector<uint32_t> uvIndicesPerSet;
	getUVSet(uvIndicesPerSet, counts, countsSize, uvCounts, uvCountsSize, uvSet, uvSets, uvIndices, uvIndicesSize);

	uint32_t vi = 0;
	for (GA_Iterator it(marker.vertexRange()); !it.atEnd(); ++it, vi++) {
		const uint32_t uvIdx = uvIndicesPerSet[vi];
		if (uvIdx == UV_IDX_NO_VALUE)
			continue;
		assert(uvIdx * 2 + 1 < uvsSize);
		const auto du = uvs[uvIdx * 2 + 0];
		const auto dv = uvs[uvIdx * 2 + 1];
		const auto u = static_cast<fpreal32>(du);
		const auto v = static_cast<fpreal32>(dv);
		handle.set(it.getOffset(), UT_Vector3F(u, v, 0.0f));
	}
}

} // namespace ModelConversion


ModelConverter::ModelConverter(GU_Detail* gdp, UT_AutoInterrupt* autoInterrupt)
: mDetail(gdp), mAutoInterrupt(autoInterrupt) { }

void ModelConverter::add(
			const wchar_t* name,
			const double* vtx, size_t vtxSize,
			const double* nrm, size_t nrmSize,
			const double* uvs, size_t uvsSize,
			const uint32_t* counts, size_t countsSize,
			const uint32_t* indices, size_t indicesSize,
			const uint32_t* uvCounts, size_t uvCountsSize,
			const uint32_t* uvIndices, size_t uvIndicesSize,
			uint32_t uvSets,
			const uint32_t* faceRanges, size_t faceRangesSize,
			const prt::AttributeMap** materials,
			const prt::AttributeMap** reports
) {
	std::lock_guard<std::mutex> guard(mDetailMutex); // protect all mDetail accesses

	// -- new prim group
	const std::string nname = toOSNarrowFromUTF16(name);
	auto& elemGroupTable = mDetail->getElementGroupTable(GA_ATTRIB_PRIMITIVE);
	GA_PrimitiveGroup* primGroup = static_cast<GA_PrimitiveGroup*>(elemGroupTable.newGroup(nname.c_str(), false));

	// -- create polygons
	const GA_Detail::OffsetMarker marker(*mDetail);
	const UTVector3FVector utPoints = convertVertices(vtx, vtxSize);
	const GEO_PolyCounts geoPolyCounts = [&counts, &countsSize](){
		GEO_PolyCounts pc;
		for (size_t ci = 0; ci < countsSize; ci++) pc.append(counts[ci]);
		return pc;
	}();
	const GA_Offset primStartOffset = GU_PrimPoly::buildBlock(mDetail, utPoints.data(), utPoints.size(), geoPolyCounts, reinterpret_cast<const int*>(indices));

	// -- vertex normals
	if (nrmSize > 0) {
		GA_RWHandleV3 nrmh(mDetail->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32));
		setVertexNormals(nrmh, marker, nrm, nrmSize, indices);
	}

	// -- texture coordinates
	for (size_t uvSet = 0; uvSet < uvSets; uvSet++) {
		GA_RWHandleV3 uvh;
		if (uvSet == 0)
		    uvh.bind(mDetail->addTextureAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32)); // adds "uv" vertex attribute
		else {
			const std::string n = "uv" + std::to_string(uvSet);
			uvh.bind(mDetail->addTuple(GA_STORE_REAL32, GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, n.c_str(), 3));
		}
		ModelConversion::setUVs(uvh, marker, counts, countsSize, uvCounts, uvCountsSize, uvSet, uvSets, uvIndices, uvIndicesSize, uvs, uvsSize);
	}

	primGroup->addRange(marker.primitiveRange());

	if (DBG) LOG_DBG << "got " << faceRangesSize-1 << " face ranges";

	// -- convert materials/reports into houdini primitive attributes based on face ranges
	if (faceRangesSize > 1) {
		HandleMaps hm;
		for (size_t fri = 0; fri < faceRangesSize-1; fri++) {
			const GA_Offset rangeStart = primStartOffset + faceRanges[fri];
			const GA_Offset rangePastEnd = primStartOffset + faceRanges[fri + 1]; // faceRanges contains faceRangeCount+1 values

			if (materials != nullptr) {
				const prt::AttributeMap* m = materials[fri];
				setupHandles(mDetail, m, hm);
				applyAttributeMap(hm, m, rangeStart, rangePastEnd);
			}

			if (reports != nullptr) {
				const prt::AttributeMap* r = reports[fri];
				setupHandles(mDetail, r, hm);
				applyAttributeMap(hm, r, rangeStart, rangePastEnd);
			}
		}
	}
}

prt::Status ModelConverter::generateError(size_t isIndex, prt::Status status, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}

prt::Status ModelConverter::assetError(size_t isIndex, prt::CGAErrorLevel level, const wchar_t* key, const wchar_t* uri, const wchar_t* message) {
	LOG_WRN << key << L": " << message;
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaError(size_t isIndex, int32_t shapeID, prt::CGAErrorLevel level, int32_t methodId, int32_t pc, const wchar_t* message) {
	LOG_ERR << message;
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaPrint(size_t isIndex, int32_t shapeID, const wchar_t* txt) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaReportBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaReportFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::cgaReportString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	return prt::STATUS_OK;
}
