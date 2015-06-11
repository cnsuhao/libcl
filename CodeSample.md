# Introduction #

The following source code illustrates how to extend libCL using the fundamental wrapper and base classes. An extension basically consists of an OpenCL program and a new C++ class derived from _oclProgram_. The .cl, .h and .cpp files of the extension are located in an appropriate subdirectory of libCL, classified according to the type of algorithm.

An extension class may instantiate buffers and associate additional algorithms as needed. The example illustrated below implements a tone mapping operator that applies to an input image and stores the result in an output image. Internally, the tone mapping class instantiates two additional buffers as well as recursive gaussian smoothing object.

## oclToneMapping.h ##

```
#include "oclRecursiveGaussian.h"

class oclToneMapping : public oclProgram
{
    public: 

        oclToneMapping(oclContext& iContext, cl_image_format iFormat);

        int compile();
        int compute(oclDevice& iDevice, oclImage2D& bfSrce, oclImage2D& bfDest);

        void setSmoothing(cl_float iValue);

    protected:

        oclRecursiveGaussian mGaussian;

        oclKernel clLuminance;
        oclKernel clCombine;

        oclImage2D bfTempA;
        oclImage2D bfTempB;
};      
```

#### Comments ####

  * oclProgram is the base class for all extensions
  * oclRecursiveGaussian is another extension class

## oclToneMapping.cpp ##

```
#include "oclToneMapping.h"

oclToneMapping::oclToneMapping(oclContext& iContext, cl_image_format iFormat)
: oclProgram(iContext, "oclToneMapping")
, bfTempA(iContext, "bfTemp0")
, bfTempB(iContext, "bfTemp1")
, clLuminance(*this)
, clCombine(*this)
, mGaussian(iContext)
{
    bfTempA.create(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, iFormat, 256, 256);
    bfTempB.create(CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, iFormat, 256, 256);

    addSourceFile("image\\oclToneMapping.cl");

    exportKernel(clLuminance);
    exportKernel(clCombine);
}

int oclToneMapping::compile()
{
    // release kernels
    clLuminance = 0;
    clCombine = 0;

    if (!mGaussian.compile())
    {
        return 0;
    }
    if (!oclProgram::compile())
    {
        return 0;
    }

    clLuminance = createKernel("clLuminance");
    KERNEL_VALIDATE(clLuminance)
    clCombine = createKernel("clCombine");
    KERNEL_VALIDATE(clCombine)
    return 1;
}

int oclToneMapping::compute(oclDevice& iDevice, oclImage2D& bfSrce, oclImage2D& bfDest)
{
    cl_uint lWidth = bfSrce.getImageInfo<size_t>(CL_IMAGE_WIDTH);
    cl_uint lHeight = bfSrce.getImageInfo<size_t>(CL_IMAGE_HEIGHT);

    if (bfTempA.getImageInfo<size_t>(CL_IMAGE_WIDTH)!=lWidth || bfTempA.getImageInfo<size_t>(CL_IMAGE_HEIGHT)!=lHeight)
    {
        bfTempA.resize(lWidth, lHeight);
    }
    if (bfTempB.getImageInfo<size_t>(CL_IMAGE_WIDTH)!=lWidth || bfTempB.getImageInfo<size_t>(CL_IMAGE_HEIGHT)!=lHeight)
    {
        bfTempB.resize(lWidth, lHeight);
    }

    size_t lGlobalSize[2];
    lGlobalSize[0] = lWidth;
    lGlobalSize[1] = lHeight;

    clSetKernelArg(clLuminance, 0, sizeof(cl_mem), bfSrce);
    clSetKernelArg(clLuminance, 1, sizeof(cl_mem), bfTempA);
    sStatusCL = clEnqueueNDRangeKernel(iDevice, clLuminance,2,NULL, lGlobalSize, NULL, 0, NULL, clLuminance.getEvent());
    ENQUEUE_VALIDATE

    if (!mGaussian.compute(iDevice, bfTempA, bfTempB, bfTempA))
    {
        return false;
    }

    clSetKernelArg(clCombine, 0, sizeof(cl_mem), bfSrce);
    clSetKernelArg(clCombine, 1, sizeof(cl_mem), bfTempA);
    clSetKernelArg(clCombine, 2, sizeof(cl_mem), bfDest);
    sStatusCL = clEnqueueNDRangeKernel(iDevice, clCombine,2,NULL, lGlobalSize, NULL, 0, NULL, clCombine.getEvent());
    ENQUEUE_VALIDATE

    return true;
}

void oclToneMapping::setSmoothing(cl_float iValue)
{
    mGaussian.setSigma(iValue);
}
```

#### Comments ####

  * KERNEL\_VALIDATE and ENQUEUE\_VALIDATE are macros
  * Buffer and kernel classes override various casting operators
  * All objects in libCL can be named for effective error reporting

## oclToneMapping.cl ##

```
__kernel void clLuminance(__read_only image2d_t RGBAin, __write_only image2d_t LUMout)
{
    const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_FILTER_NEAREST | CLK_ADDRESS_CLAMP;

    const int gx = get_global_id(0);
    const int gy = get_global_id(1);
    const int gw = get_global_size(0);

    float4 RGBA = read_imagef(RGBAin, sampler, (float2)(gx,gy));

    write_imagef(LUMout, (int2)(gx,gy), dot((float4)(0.2126,0.7152,0.0722,0.0),RGBA));
}

__kernel void clCombine(__read_only image2d_t RGBDin, __read_only image2d_t LUMin, __write_only image2d_t RGBDout)
{
    const sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_FILTER_LINEAR | CLK_ADDRESS_CLAMP;

    const int gx = get_global_id(0);
    const int gy = get_global_id(1);
    const int gw = get_global_size(0);
    const int gh = get_global_size(1);

    float2 pixel = (float2)((gx+0.5f)/gw,(gy+0.5f)/gh);

    float4 RGBA = read_imagef(RGBDin, sampler, pixel);
    float4 LLLL = read_imagef(LUMin, sampler, pixel);

    float luminance = dot((float4)(0.2126f,0.7152f,0.0722f,0.0f),RGBA);

    float glare = 0.0f;//log(l) * (1.0f - 0.99f/(0.99f + log(l)));

    // Retinex
    float4 result = RGBA*(exp(glare + log(luminance)-0.45f*log(LLLL)));

    write_imagef(RGBDout, (int2)(gx,gy), result);
}
```
#### Comments ####

  * The kernels names must be the same as in the call to _createKernel_
  * The tone mapping algorithm still needs to be refined