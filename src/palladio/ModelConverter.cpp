#include "ModelConverter.h"
#include "AttributeConversion.h"
#include "ShapeConverter.h"
#include "LogHandler.h"
#include "MultiWatch.h"

#include "GU/GU_HoleInfo.h"

#include "boost/variant.hpp"

#include <mutex>


namespace {

constexpr bool DBG = false;

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

std::mutex mDetailMutex; // guard the houdini detail object

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
		for (size_t u = 0; u < uvSets; u++)
			uvIdxBase += uvCounts[c*uvSets + u];
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

GA_Offset createPrimitives(GU_Detail* mDetail, GenerateNodeParams::GroupCreation gc, const wchar_t* name,
                           const double* vtx, size_t vtxSize,
                           const double* nrm, size_t nrmSize,
                           const double* uvs, size_t uvsSize,
                           const uint32_t* counts, size_t countsSize,
                           const uint32_t* indices, size_t indicesSize,
                           const uint32_t* uvCounts, size_t uvCountsSize,
                           const uint32_t* uvIndices, size_t uvIndicesSize, uint32_t uvSets)
{
	WA("all");

	// -- create primitives
	const GA_Detail::OffsetMarker marker(*mDetail);
	const UTVector3FVector utPoints = convertVertices(vtx, vtxSize);
	const GEO_PolyCounts geoPolyCounts = [&counts, &countsSize]() {
		GEO_PolyCounts pc;
		for (size_t ci = 0; ci < countsSize; ci++) pc.append(counts[ci]);
		return pc;
	}();
	const GA_Offset primStartOffset = GU_PrimPoly::buildBlock(mDetail, utPoints.data(), utPoints.size(),
	                                                          geoPolyCounts, reinterpret_cast<const int*>(indices));

	// -- add vertex normals
	if (nrmSize > 0) {
		GA_RWHandleV3 nrmh(mDetail->addNormalAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32));
		setVertexNormals(nrmh, marker, nrm, nrmSize, indices);
	}

	// -- add texture coordinates
	for (size_t uvSet = 0; uvSet < uvSets; uvSet++) {
		GA_RWHandleV3 uvh;
		if (uvSet == 0)
			uvh.bind(mDetail->addTextureAttribute(GA_ATTRIB_VERTEX, GA_STORE_REAL32)); // adds "uv" vertex attribute
		else {
			const std::string n = "uv" + std::to_string(uvSet);
			uvh.bind(mDetail->addTuple(GA_STORE_REAL32, GA_ATTRIB_VERTEX, GA_SCOPE_PUBLIC, n.c_str(), 3));
		}
		ModelConversion::setUVs(uvh, marker, counts, countsSize, uvCounts, uvCountsSize,
		                        uvSet, uvSets, uvIndices, uvIndicesSize, uvs, uvsSize);
	}

	// -- optionally create primitive groups
	if (gc == GenerateNodeParams::GroupCreation::PRIMCLS) {
		const std::string nName = toOSNarrowFromUTF16(name);
		auto& elemGroupTable = mDetail->getElementGroupTable(GA_ATTRIB_PRIMITIVE);
		GA_PrimitiveGroup* primGroup = static_cast<GA_PrimitiveGroup*>(elemGroupTable.newGroup(nName.c_str(), false));
		primGroup->addRange(marker.primitiveRange());
	}

	return primStartOffset;
}

} // namespace ModelConversion


ModelConverter::ModelConverter(GU_Detail* detail, GenerateNodeParams::GroupCreation gc, UT_AutoInterrupt* autoInterrupt)
: mDetail(detail), mGroupCreation(gc), mAutoInterrupt(autoInterrupt) { }

void ModelConverter::add(const wchar_t* name,
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
                         const prt::AttributeMap** reports,
                         const int32_t* shapeIDs)
{
	// we need to protect mDetail, it is accessed by multiple generate threads
	std::lock_guard<std::mutex> guard(mDetailMutex);

	const GA_Offset primStartOffset = ModelConversion::createPrimitives(mDetail, mGroupCreation, name,
	                                                                    vtx, vtxSize, nrm, nrmSize, uvs, uvsSize,
	                                                                    counts, countsSize, indices, indicesSize,
	                                                                    uvCounts, uvCountsSize,
	                                                                    uvIndices, uvIndicesSize, uvSets);

	// -- convert materials/reports into primitive attributes based on face ranges
	if (DBG) LOG_DBG << "got " << faceRangesSize-1 << " face ranges";
	if (faceRangesSize > 1) {
		WA("add materials/reports");

		AttributeConversion::HandleMap handleMap;
		const GA_IndexMap& primIndexMap = mDetail->getIndexMap(GA_ATTRIB_PRIMITIVE);
		for (size_t fri = 0; fri < faceRangesSize-1; fri++) {
			const GA_Offset rangeStart = primStartOffset + faceRanges[fri];
			const GA_Size   rangeSize  = faceRanges[fri + 1] - faceRanges[fri];

			if (materials != nullptr) {
				const prt::AttributeMap* attrMap = materials[fri];
				AttributeConversion::extractAttributeNames(handleMap, attrMap);
				AttributeConversion::createAttributeHandles(mDetail, handleMap);
				AttributeConversion::setAttributeValues(handleMap, attrMap, primIndexMap, rangeStart, rangeSize);
			}

			if (reports != nullptr) {
				const prt::AttributeMap* attrMap = reports[fri];
				AttributeConversion::extractAttributeNames(handleMap, attrMap);
				AttributeConversion::createAttributeHandles(mDetail, handleMap);
				AttributeConversion::setAttributeValues(handleMap, attrMap, primIndexMap, rangeStart, rangeSize);
			}

			if (!mShapeAttributeBuilders.empty()) {
				// implicit contract: the attr{Bool,Float,String} callbacks are called prior to ModelConverter::add
				const int32_t shapeID = shapeIDs[fri];
				auto it = mShapeAttributeBuilders.find(shapeID);
				if (it != mShapeAttributeBuilders.end()) {
					const AttributeMapUPtr attrMap(it->second->createAttributeMap());
					AttributeConversion::extractAttributeNames(handleMap, attrMap.get());
					AttributeConversion::createAttributeHandles(mDetail, handleMap);
					AttributeConversion::setAttributeValues(handleMap, attrMap.get(), primIndexMap,
					                                        rangeStart, rangeSize);
				}
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

namespace {

AttributeMapBuilderUPtr& getBuilder(std::map<int32_t, AttributeMapBuilderUPtr>& amb, int32_t shapeID) {
	auto it = amb.find(shapeID);
	if (it == amb.end())
		it = amb.emplace(shapeID, std::move(AttributeMapBuilderUPtr(prt::AttributeMapBuilder::create()))).first;
	return it->second;
}

} // namespace

prt::Status ModelConverter::attrBool(size_t isIndex, int32_t shapeID, const wchar_t* key, bool value) {
	getBuilder(mShapeAttributeBuilders, shapeID)->setBool(key, value);
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrFloat(size_t isIndex, int32_t shapeID, const wchar_t* key, double value) {
	getBuilder(mShapeAttributeBuilders, shapeID)->setFloat(key, value);
	return prt::STATUS_OK;
}

prt::Status ModelConverter::attrString(size_t isIndex, int32_t shapeID, const wchar_t* key, const wchar_t* value) {
	getBuilder(mShapeAttributeBuilders, shapeID)->setString(key, value);
	return prt::STATUS_OK;
}
