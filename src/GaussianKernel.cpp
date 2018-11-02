//
//  GaussianKernel.cpp
//  FluidMultiThreadCPU
//
//  Created by Anthony Setyawan on 11/1/18.
//
//

#include "GaussianKernel.hpp"

inline float gaussianDistribution (float x, float mu, float sigma)
{
    float d = x - mu;
    float n = 1.0 / (sqrt(2 * 3.14) * sigma);
    return exp(-d*d/(2 * sigma * sigma)) * n;
};

template<typename F>
inline vector<pair<float, float>> sampleInterval(F f, float minInclusive, float maxInclusive, int sampleCount)
{
    vector<pair<float, float>> result(sampleCount);
    float stepSize = (maxInclusive - minInclusive) / (sampleCount-1);
    
    for(int s=0; s<sampleCount; ++s)
    {
        float x = minInclusive + s * stepSize;
        float y = f(x);
        
        result[s] = make_pair(x, y);
    }
    
    return result;
};

inline float integrateSimphson(vector<pair<float, float>>& samples)
{
    float result = samples[0].second + samples[samples.size()-1].second;
    
    for(int s = 1; s < samples.size()-1; ++s)
    {
        float sampleWeight = (s % 2 == 0) ? 2.0 : 4.0;
        result += sampleWeight * samples[s].second;
    }
    
    float h = (samples[samples.size()-1].first - samples[0].first) / (samples.size()-1);
    return result * h / 3.0;
};

inline vector<float> gaussianFunction(float sigma, int kernelSize, int sampleCount) {
    int samplesPerBin = ceil(sampleCount / kernelSize);
    if(samplesPerBin % 2 == 0) // need an even number of intervals for simpson integration => odd number of samples
        ++samplesPerBin;
    
    float kernelLeft = -floor((float)kernelSize/2);
    
    auto calcSamplesForRange = [=](float minInclusive, float maxInclusive)
    {
        return sampleInterval(
                              [=](float x) {
                                  return gaussianDistribution(x, 0, sigma);
                              },
                              minInclusive,
                              maxInclusive,
                              samplesPerBin
                              );
    };
    
    vector<float> result(kernelSize);
    float weightSum = 0;
    // now sample kernel taps and calculate tap weights
    for(int tap=0; tap<kernelSize; ++tap)
    {
        float left = kernelLeft - 0.5 + tap;
        
        vector<pair<float, float>> tapSamples = calcSamplesForRange(left, left+1);
        float tapWeight = integrateSimphson(tapSamples);
        weightSum += tapWeight;
        result[tap] = tapWeight;
    }
    
    for (int i = 0; i < result.size(); ++i) {
        result[i] /= weightSum;
    }
    return result;
}

vector<float> GaussianKernel::generate(GaussianKernelConfig& config) {
    if (isCacheSet) {
        if (prevConfig == config) {
            return cachedResult;
        }
    }
    prevConfig = config;
    return cachedResult = gaussianFunction(config.sigma, config.kernelSize, config.sampleCount);
}

