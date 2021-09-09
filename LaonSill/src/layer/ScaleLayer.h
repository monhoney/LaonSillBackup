/*
 * ScaleLayer.h
 *
 *  Created on: Jan 6, 2018
 *      Author: jkim
 */

#ifndef SCALELAYER_H_
#define SCALELAYER_H_

#include "common.h"
#include "LearnableLayer.h"
#include "BiasLayer.h"
#include "LayerFunc.h"

template <typename Dtype>
class ScaleLayer : public LearnableLayer<Dtype> {
public:
	ScaleLayer();
	virtual ~ScaleLayer();

	virtual void backpropagation();
	virtual void reshape();
	virtual void feedforward();

	virtual void update();
	void applyChanges(LearnableLayer<Dtype> *targetLayer);
	void syncParams(LearnableLayer<Dtype> *targetLayer);


private:
	BiasLayer<Dtype>* buildBiasLayer();

private:
	Data<Dtype> sumMultiplier;
	Data<Dtype> sumResult;
	Data<Dtype> temp;

	int axis;
	int outerDim;
	int scaleDim;
	int innerDim;

	bool biasTerm;

	int biasParamId;

	std::vector<update_param> updatePolicies;

	BiasLayer<Dtype>* biasLayer;
	Data<Dtype> biasInput;
	Data<Dtype> biasOutput;



public:
    /****************************************************************************
     * layer callback functions
     ****************************************************************************/
    static void* initLayer();
    static void destroyLayer(void* instancePtr);
    static void setInOutTensor(void* instancePtr, void* tensorPtr, bool isInput, int index);
    static bool allocLayerTensors(void* instancePtr);
    static void forwardTensor(void* instancePtr, int miniBatchIndex);
    static void backwardTensor(void* instancePtr);
    static void learnTensor(void* instancePtr);
    static bool checkShape(std::vector<TensorShape> inputShape,
            std::vector<TensorShape> &outputShape);
    static uint64_t calcGPUSize(std::vector<TensorShape> inputShape);

};

#endif /* SCALELAYER_H_ */
