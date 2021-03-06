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

#include "bslam/utils/Convenience.h"
#include "bslam/utils/Config.h"
#include "bslam/utils/Log.h"
#include "bslam/utils/Stopwatch.h"


namespace bslam {

template<typename SLAM>
CarmenProcessor<SLAM>::CarmenProcessor( SLAM *slam, std::istream &inputStream, std::istream *gtInputStream ) :
	m_slam( slam ),
	m_reader( inputStream ),
	m_laserStartAngle( BSLAM_DEG2RAD( Config::getDouble( "LASER_START_ANGLE", -90.0 ) ) ),
	m_laserAngularRes( BSLAM_DEG2RAD( Config::getDouble( "LASER_ANGULAR_RES",   1.0 ) ) ),
	m_first( true ),
	m_step( 0 ),
	m_saveEachStep( Config::getInt( "SAVE_EACH_STEP", 0 ) ),
	m_outputDir( Config::get( "OUTPUT_DIR", "." ) )
{
	if( gtInputStream ) {
		m_gtReader = new CarmenGtReader( *gtInputStream );
	} else {
		m_gtReader = nullptr;
	}
}


template<typename SLAM>
CarmenProcessor<SLAM>::~CarmenProcessor() {
	if( m_gtReader ) {
		delete m_gtReader;
	}
}


template<typename SLAM>
bool
CarmenProcessor<SLAM>::processNext( bool *scanProcessed ) {
	PoseSE2 								pose;
	std::vector<double> 					scan;
	std::string								timestamp;
	std::vector<typename Scan::ConstPtr>	scanVector;

	scanVector.push_back( typename Scan::ConstPtr( new Scan ) );

	if( m_reader.nextReading( &pose, &scan, &timestamp ) ) {
		if( m_first ) {
			m_lastPose = pose;
			m_first	 = false;
		}

		// Process odometry
		m_slam->processOdom( m_lastPose, pose );
		m_lastPose = pose;

		if( m_slam->scanRequired() ) {
			// Convert laser scan to 2D point cloud
			typename Scan::Ptr scanPointCloud( new Scan );
			scanPointCloud->reserve( scan.size() );
			for( size_t i = 0; i < scan.size(); i++ ) {
				double angle = m_laserStartAngle + m_laserAngularRes*i;

				scanPointCloud->push_back( PointType( scan[i] * cos( angle ), scan[i] * sin( angle ) ) );
			}

			scanVector[0] = scanPointCloud;

			// Process scan
			m_slam->processScan( scanVector );

			// Save timestamp
			m_timestamps.push_back( timestamp );

			// Save map and best path
			if( m_saveEachStep != 0 && (m_step % m_saveEachStep) == 0 ) {
				size_t 		bestIdx		= m_slam->bestParticleIdx( false );

				std::string	stepStr		= std::to_string( m_step );
				while( stepStr.size() < 5 ) {
					stepStr = "0" + stepStr;
				}

				l_inf( "Saving best map..." );
				const auto &map = m_slam->map( bestIdx, false );
				std::ofstream mapFile( m_outputDir + "/map_" + stepStr + ".map" );
				map.save( mapFile );
				mapFile.close();

				l_inf( "Saving best trajectory..." );
				const auto &traj = m_slam->trajectory( bestIdx, false );
				std::ofstream trajFile( m_outputDir + "/trajectory_" + stepStr + ".csv" );
				trajFile << "# x[m] y[m] phi[deg]" << std::endl;
				for( auto &pose : traj ) {
					trajFile << pose << std::endl;
				}
				trajFile.close();
			}
			m_step++;

			// Find GT pose and calc error
			if( m_gtReader ) {
				PoseSE2 		gtPose;
				std::string		gtTimestamp;
				bool			found = false;

				while( !found && m_gtReader->nextPose( &gtPose, &gtTimestamp ) ) {
					if( gtTimestamp == timestamp ) {
						found = true;
					}
				}

				if( found ) {

					// TODO: Other error calculation?

					/*
					// Error between absolute pose
					PoseSE2 estPose = m_slam->pose( m_slam->bestParticleIdx() );

					double error = 0;
					for( size_t i = 0; i < 2; i++ )
						error += SQR( estPose.pos[i] - gtPose.pos[i] );
					error = sqrt( error );
					*/

					/*
					// Relative diff between last and current pose for current best particle
					Trajectory estTraj = m_slam->trajectory( m_slam->bestParticleIdx(), (size_t) 2 );
					PoseSE2 	&estPose	= estTraj[1],
							estPoseDiff = estTraj[0].ominus( estTraj[1] ),
							gtPoseDiff	= m_lastGtPose.ominus( gtPose );

					double error = 0;
					for( size_t i = 0; i < 2; i++ )
						error += SQR( estPoseDiff.pos[i] - gtPoseDiff.pos[i] );
					error = sqrt( error );
					*/

					// Relative diff between last and current pose for current and last best particle
					PoseSE2 estPose 	= m_slam->pose( m_slam->bestParticleIdx( false ) ),
							estPoseDiff = m_lastEstPose.ominus( estPose ),
							gtPoseDiff	= m_lastGtPose.ominus( gtPose );

					double error = 0;
					for( size_t i = 0; i < 2; i++ ) {
						error += SQR( estPoseDiff.pos()[i] - gtPoseDiff.pos()[i] );
					}
					error = sqrt( error );


					m_gtTrajectory.push_back( gtPose );
					m_poseError.push_back( error );

					m_lastGtPose 	= gtPose;
					m_lastEstPose	= estPose;

					l_inf( "Estimated pose:   \t" << estPose );
					l_inf( "Ground truth pose:\t" << gtPose );
					l_inf( "Pose error:       \t" << error );
				} else {
					l_wrn( "No GT pose for timestamp " << timestamp << ". Unable to compute error.");
				}
			}

			if( scanProcessed ) {
				*scanProcessed = true;
			}
		} else {
			if( scanProcessed ) {
				*scanProcessed = false;
			}
		}

		return true;
	} else {
		return false;
	}
}


template<typename SLAM>
uint64_t
CarmenProcessor<SLAM>::processAll() {
	uint64_t	count = 0;
	bool		scanProcessed;
	Stopwatch	stopWatch;

	while( processNext( &scanProcessed ) ) {
		if( scanProcessed ) {
			count++;
			l_inf( "Total step time:            " << stopWatch.timePast() );
			stopWatch.reset();
		}
	}

	return count;
}

} /* namespace bslam */
