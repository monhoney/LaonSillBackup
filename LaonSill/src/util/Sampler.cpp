#include <algorithm>
#include <vector>

#include "Sampler.h"
#include "MathFunctions.h"
#include "SysLog.h"
#include "BBoxUtil.h"


using namespace std;

void GroupObjectBBoxes(const AnnotatedDatum& annoDatum,
                       vector<NormalizedBBox>* objectBBoxes) {
	objectBBoxes->clear();
	for (int i = 0; i < annoDatum.annotation_groups.size(); i++) {
		const AnnotationGroup& annoGroup = annoDatum.annotation_groups[i];
		for (int j = 0; j < annoGroup.annotations.size(); j++) {
			const Annotation_s& anno = annoGroup.annotations[j];
			objectBBoxes->push_back(anno.bbox);
		}
	}
}

bool SatisfySampleConstraint(const NormalizedBBox& sampledBBox,
                             const vector<NormalizedBBox>& objectBBoxes,
                             const SampleConstraint& sampleConstraint) {
	bool hasJaccardOverlap = sampleConstraint.hasMinJaccardOverlap() ||
			sampleConstraint.hasMaxJaccardOverlap();
	bool hasSampleCoverage = sampleConstraint.hasMinSampleCoverage() ||
			sampleConstraint.hasMaxSampleCoverage();
	bool hasObjectCoverage = sampleConstraint.hasMinObjectCoverage() ||
			sampleConstraint.hasMaxObjectCoverage();
	bool satisfy = !hasJaccardOverlap && !hasSampleCoverage && !hasObjectCoverage;

	if (satisfy) {
		// By default, the sampleBBox is "positive" if no constraints are defined.
		return true;
	}
	// Check constraints.
	bool found = false;
	for (int i = 0; i < objectBBoxes.size(); i++) {
		const NormalizedBBox& objectBBox = objectBBoxes[i];
		// Test jaccard overlap
		if (hasJaccardOverlap) {
			const float jaccardOverlap = JaccardOverlap(sampledBBox, objectBBox);
			if (sampleConstraint.hasMinJaccardOverlap() &&
					jaccardOverlap < sampleConstraint.minJaccardOverlap) {
				continue;
			}
			if (sampleConstraint.hasMaxJaccardOverlap() &&
					jaccardOverlap > sampleConstraint.maxJaccardOverlap) {
				continue;
			}
			found = true;
		}
		// Test sample coverage.
		if (hasSampleCoverage) {
			const float sampleCoverage = BBoxCoverage(sampledBBox, objectBBox);
			if (sampleConstraint.hasMinSampleCoverage() &&
					sampleCoverage < sampleConstraint.minSampleCoverage) {
				continue;
			}
			if (sampleConstraint.hasMaxSampleCoverage() &&
					sampleCoverage > sampleConstraint.maxSampleCoverage) {
				continue;
			}
			found = true;
		}
		// Test object coverage.
		if (hasObjectCoverage) {
			const float objectCoverage = BBoxCoverage(objectBBox, sampledBBox);
			if (sampleConstraint.hasMinObjectCoverage() &&
					objectCoverage < sampleConstraint.minObjectCoverage) {
				continue;
			}
			if (sampleConstraint.hasMaxObjectCoverage() &&
					objectCoverage > sampleConstraint.maxObjectCoverage) {
				continue;
			}
			found = true;
		}
		if (found) {
			return true;
		}
	}
	return found;
}

// ?????? bbox ?????? Sampler??? ?????? (scale, aspectRatio)??? ???????????? bbox??? ??????
void SampleBBox(const Sampler& sampler, NormalizedBBox* sampledBBox) {
	// Get random scale.
	SASSERT0(sampler.maxScale >= sampler.minScale);
	SASSERT0(sampler.minScale > 0.f);
	SASSERT0(sampler.maxScale <= 1.f);
	float scale;
	soooa_rng_uniform(1, sampler.minScale, sampler.maxScale, &scale);

	// Get random aspect ratio
	SASSERT0(sampler.maxAspectRatio >= sampler.minAspectRatio);
	SASSERT0(sampler.minAspectRatio > 0.f);
	SASSERT0(sampler.maxAspectRatio < FLT_MAX);
	float aspectRatio;
	float minAspectRatio = std::max<float>(sampler.minAspectRatio, std::pow(scale, 2.f));
	float maxAspectRatio = std::min<float>(sampler.maxAspectRatio, 1 / std::pow(scale, 2.f));
	soooa_rng_uniform(1, minAspectRatio, maxAspectRatio, &aspectRatio);

	// 0 ~ 1 ????????? ?????? ????????? ?????? bbox ????????? ??????
	// Figure out bbox dimension.
	// bboxWidth / bboxHeight = aspectRatio
	// ?????? scale??? aspectRatio??? ???????????? bbox width / height ??????
	float bboxWidth = std::min<float>(scale * sqrt(aspectRatio), 1.f);
	float bboxHeight = std::min<float>(scale / sqrt(aspectRatio), 1.f);

	// (0, 0) ???????????? woff, hoff?????? shift
	// ????????? 0 ~ 1 ???????????? NormalizedBBox ??????
	// Figure out top left coordinates.
	float wOff, hOff;
	soooa_rng_uniform(1, 0.f, 1 - bboxWidth, &wOff);
	soooa_rng_uniform(1, 0.f, 1 - bboxHeight, &hOff);

	// NormalizedBBox ????????? ????????? ????????? BBox(scale, aspectRatio??? ??????)??? ??????
	// ????????? ????????? sampler??? ?????????...
	sampledBBox->xmin = wOff;
	sampledBBox->ymin = hOff;
	sampledBBox->xmax = wOff + bboxWidth;
	sampledBBox->ymax = hOff + bboxHeight;
}

void GenerateSamples(const NormalizedBBox& sourceBBox,
                     const vector<NormalizedBBox>& objectBBoxes,
                     const BatchSampler& batchSampler,
                     vector<NormalizedBBox>* sampledBBoxes) {
	int found = 0;
	// ?????? max trial?????? try
	for (int i = 0; i < batchSampler.maxTrials; i++) {
		// ?????? max sample ???????????? sample
		if (batchSampler.hasMaxSample() &&
				found >= batchSampler.maxSample) {
			break;
		}
		// Generate sampledBBox in the normalized space [0, 1]
		NormalizedBBox sampledBBox;
		// sampler ???????????? ????????? sampled bbox??? ??????
		// ?????? bbox ?????? Sampler??? ?????? (scale, aspectRatio)??? ???????????? bbox??? ??????
		// ??? ????????? ?????? ?????? sampler ????????? ???????????? ????????? bbox ??????
		SampleBBox(batchSampler.sampler, &sampledBBox);
		// Transform the sampledBBox w.r.t. sourceBBox.
		LocateBBox(sourceBBox, sampledBBox, &sampledBBox);
		// Determine if the sampled bbox is positive or negative by the constraint.
		if (SatisfySampleConstraint(sampledBBox, objectBBoxes, batchSampler.sampleConstraint)) {
			found++;
			sampledBBoxes->push_back(sampledBBox);
		}
	}
}

void GenerateBatchSamples(const AnnotatedDatum& annoDatum,
                          const vector<BatchSampler>& batchSamplers,
                          vector<NormalizedBBox>* sampledBBoxes) {
	sampledBBoxes->clear();
	vector<NormalizedBBox> objectBBoxes;
	// annoDatum?????? group??? ????????? ?????? bbox??? vector??? ??????
	GroupObjectBBoxes(annoDatum, &objectBBoxes);
	for (int i = 0; i < batchSamplers.size(); i++) {
		if (batchSamplers[i].useOriginalImage) {
			NormalizedBBox unitBBox;
			unitBBox.xmin = 0.f;
			unitBBox.ymin = 0.f;
			unitBBox.xmax = 1.f;
			unitBBox.ymax = 1.f;
			// unitBBox: ?????? BBox
			// objectBBoxes: annoDatum??? ?????? BBox ?????? Vector
			// batchSamplers[i]: ?????? batch sampler
			// sampledBBoxes: batch sampler??? ????????? sample??? BBoxes
			GenerateSamples(unitBBox, objectBBoxes, batchSamplers[i], sampledBBoxes);
		}
	}
}
