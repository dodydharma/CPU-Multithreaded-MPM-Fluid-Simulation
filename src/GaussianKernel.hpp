//
//  GaussianKernel.hpp
//  FluidMultiThreadCPU
//
//  Created by Anthony Setyawan on 11/1/18.
//
//
// Provides 1D Gaussian kernel with cached result

#ifndef GaussianKernel_hpp
#define GaussianKernel_hpp

#include <stdio.h>

using namespace std;

typedef struct X {
    float sigma = 5.0f;
    int kernelSize = 21;
    int sampleCount = 1000;
    bool operator==(X& x)
    {
        return
            this->sigma == x.sigma &&
            this->kernelSize == x.kernelSize &&
            this->sampleCount == x.sampleCount
        ;
    }
} GaussianKernelConfig;

class GaussianKernel {
public:
    // Return 1D Gaussian kernel with standard deviation sigma and total sampling count sampleCount. Note that kernelSize must be odd
    vector<float> generate(GaussianKernelConfig&);
    
private:
    vector<float> cachedResult;
    bool isCacheSet = false;
    GaussianKernelConfig prevConfig;
};

#endif /* GaussianKernel_hpp */
