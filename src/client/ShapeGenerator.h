#pragma once

#include "PRTContext.h"
#include "ShapeConverter.h"
#include "utils.h"


class GU_Detail;

struct ShapeGenerator final : ShapeConverter {
    void get(const GU_Detail* detail, const PrimitiveClassifier& primCls, ShapeData& shapeData, const PRTContextUPtr& prtCtx) override;
};
