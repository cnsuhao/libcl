// Copyright [2011] [Geist Software Labs Inc.]
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _oclBilateralGaussian
#define _oclBilateralGaussian

#include "oclProgram.h"
#include "oclImage2D.h"

class oclBilateralGaussian : public oclProgram
{
    public: 

	    oclBilateralGaussian(oclContext& iContext);

		int compile();
		int compute(oclDevice& iDevice, oclImage2D& bfSource, oclImage2D& bfDest);

		void setRadius(cl_uint iValue);
		void setScalar(cl_float iValue);

    protected:

		oclKernel clBilateralGaussian;

    private:

		size_t mLocalSize[2];
};      



#endif