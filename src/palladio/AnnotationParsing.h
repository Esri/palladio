/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "Utils.h"

#include "prt/Annotation.h"

namespace AnnotationParsing {

enum class AttributeTrait { ENUM, RANGE, ANGLE, PERCENT, FILE, DIR, COLOR, DESCRIPTION, NONE };

struct EnumAnnotation {
	bool mRestricted;
	std::wstring mValuesAttr;
	std::vector<std::wstring> mOptions;
};

using RangeAnnotation = std::pair<double, double>;
using FileAnnotation = std::vector<std::wstring>;
using ColorAnnotation = std::array<double, 3>;

struct AttributeAnnotationInfo {
	const prt::Annotation* mAnnotation;
	AttributeTrait mAttributeTrait;
	std::wstring mDescription;
};

ColorAnnotation parseColor(const std::wstring colorString);

RangeAnnotation parseRangeAnnotation(const prt::Annotation* mAnnotation);

EnumAnnotation parseEnumAnnotation(const prt::Annotation* mAnnotation);

FileAnnotation parseFileAnnotation(const prt::Annotation* mAnnotation);

AttributeTrait detectAttributeTrait(const prt::Annotation* mAnnotation);

AttributeAnnotationInfo getAttributeAnnotationInfo(const std::wstring& attributeName, const RuleFileInfoUPtr& info);
}
