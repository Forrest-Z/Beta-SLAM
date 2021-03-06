/*
 * Software License Agreement (BSD License)
 *
 *  Beta-SLAM - Simultaneous localization and grid mapping with beta distributions
 *  Copyright (c) 2013-2019, Joachim Clemens, Thomas Reineking, Tobias Kluth
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of BSLAM nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BSLAM_POSEBASE_H_
#define BSLAM_POSEBASE_H_

#include "Point.h"


namespace bslam {

// Forward declaration
class PoseSE2;
class PoseSE3;

template<int N>
struct PoseSE {
};

template<>
struct PoseSE<2> {
	using Type = PoseSE2;
};

template<>
struct PoseSE<3> {
	using Type = PoseSE3;
};


class PoseBase {
public:
	using	EigenIsometry2w		= Eigen::Transform<world_t, 2, Eigen::Isometry>;

	using	world_other_t		= typename std::conditional<std::is_same<world_t, double>::value, float, double>::type;
	using	EigenIsometry2wo	= Eigen::Transform<world_other_t, 2, Eigen::Isometry>;

			inline				PoseBase() {};
	virtual inline				~PoseBase() {};

	template<int N>
	inline	typename PoseSE<N>::Type toPoseSE() const;
			inline	operator	PoseSE2() const;
	virtual			PoseSE2		toPoseSE2() const 			= 0;

	virtual			operator	EigenIsometry2w() const		= 0;

	virtual	inline	operator	EigenIsometry2wo() const;

	template<int N>
			inline	Pointw<N>	pos() const;
	virtual 		Point2w		pos2D() const 				= 0;
	virtual 		Point3w		pos3D() const 				= 0;

	virtual 		double		posNorm() const 			= 0;
	virtual 		double		angNorm() const 			= 0;


	virtual inline	int			dimension() const noexcept	{ return 0; }
};

} /* namespace bslam */

#ifndef NO_HPP
#include "PoseBase.hpp"
#endif

#endif /* BSLAM_POSEBASE_H_ */
