/**
 * @file HyperTangentLayer.h
 * @date 2017-03-03
 * @author moonhoen lee
 * @brief 
 * @details
 */

#ifndef HYPERTANGENTLAYER_H
#define HYPERTANGENTLAYER_H 

#include "common.h"
#include "LearnableLayer.h"
#include "LayerConfig.h"
#include "LayerFunc.h"

template<typename Dtype>
class HyperTangentLayer : public Layer<Dtype> {
public: 
	/**
	 * @details FullyConnectedLayer 기본 생성자
	 *          내부적으로 레이어 타입만 초기화한다.
	 */
	HyperTangentLayer();
    virtual ~HyperTangentLayer() {}

	virtual void backpropagation();
	virtual void reshape();
	virtual void feedforward();

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

#endif /* HYPERTANGENTLAYER_H */
