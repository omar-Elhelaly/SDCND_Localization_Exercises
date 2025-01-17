// Udacity SDC C3 Localization
// Dec 21 2020
// Aaron Brown

using namespace std;
#include <string>
#include <pcl/io/pcd_io.h>
#include <pcl/visualization/pcl_visualizer.h>
#include <pcl/filters/voxel_grid.h>
#include "helper.h"
#include <sstream>
#include <chrono> 
#include <ctime> 
#include <pcl/registration/icp.h>
#include <pcl/registration/ndt.h>
#include <pcl/console/time.h>

enum Registration{ Off, Icp};
Registration matching = Off;

Pose pose(Point(0,0,0), Rotate(0,0,0));
Pose savedPose = pose;
void keyboardEventOccurred(const pcl::visualization::KeyboardEvent &event, void* viewer)
{
	if (event.getKeySym() == "Right" && event.keyDown()){
		matching = Off;
		pose.position.x += 0.1;
  	}
	else if (event.getKeySym() == "Left" && event.keyDown()){
		matching = Off;
		pose.position.x -= 0.1;
  	}
  	else if (event.getKeySym() == "Up" && event.keyDown()){
		matching = Off;
		pose.position.y += 0.1;
  	}
	else if (event.getKeySym() == "Down" && event.keyDown()){
		matching = Off;
		pose.position.y -= 0.1;
  	}
	else if (event.getKeySym() == "k" && event.keyDown()){
		matching = Off;
		pose.rotation.yaw += 0.1;
		while( pose.rotation.yaw > 2*pi)
			pose.rotation.yaw -= 2*pi; 
  	}
	else if (event.getKeySym() == "l" && event.keyDown()){
		matching = Off;
		pose.rotation.yaw -= 0.1;
		while( pose.rotation.yaw < 0)
			pose.rotation.yaw += 2*pi; 
  	}
	else if(event.getKeySym() == "i" && event.keyDown()){
		matching = Icp;
	}
	else if(event.getKeySym() == "space" && event.keyDown()){
		matching = Off;
		pose = savedPose;
	}
	else if(event.getKeySym() == "x" && event.keyDown()){
		matching = Off;
		savedPose = pose;
	}

}

Eigen::Matrix4d ICP(PointCloudT::Ptr target, PointCloudT::Ptr source, Pose startingPose, int iterations){

  	Eigen::Matrix4d transformation_matrix = Eigen::Matrix4d::Identity ();

  	// TODO: Implement the PCL ICP function and return the correct transformation matrix
	// 1. Convert pose to transformation matrix <--- helper.cpp
	Eigen::Matrix4d initTransform = transform3D(startingPose.rotation.yaw, startingPose.rotation.pitch, startingPose.rotation.roll,
		startingPose.position.x, startingPose.position.y, startingPose.position.z);

	// 2. Transform the source to the startingPose
	PointCloudT::Ptr transformSource(new PointCloudT);
	pcl::transformPointCloud(*source, *transformSource, initTransform);
	pcl::console::TicToc time;
	time.tic();

	// 3. Create the PCL icp object
	pcl::IterativeClosestPoint<PointT, PointT> icp;

	// 4. Set the icp object's values
	icp.setMaximumIterations(iterations);
	icp.setInputSource(transformSource);
	icp.setInputTarget(target);
	icp.setMaxCorrespondenceDistance(2);

	PointCloudT::Ptr cloud_icp(new PointCloudT);  // ICP output point cloud

	// 5. Call align on the icp object
	icp.align(*cloud_icp);

	// 6. If icp converged get the icp objects output transform and adjust it by the startingPose, return the adjusted transform
	if (icp.hasConverged())
	{
		std::cout << "\nICP has converged, score is " << icp.getFitnessScore() << std::endl;
		transformation_matrix = icp.getFinalTransformation().cast<double>();
		transformation_matrix = transformation_matrix * initTransform;
		return transformation_matrix;
	}
	// 7. If icp did not converge log the message and return original identity matrix
	else 
		cout << "WARNING: ICP did not converge" << endl;

  	return transformation_matrix;

}

void drawCar(Pose pose, int num, Color color, double alpha, pcl::visualization::PCLVisualizer::Ptr& viewer){

	BoxQ box;
	box.bboxTransform = Eigen::Vector3f(pose.position.x, pose.position.y, 0);
    box.bboxQuaternion = getQuaternion(pose.rotation.yaw);
    box.cube_length = 4;
    box.cube_width = 2;
    box.cube_height = 2;
	renderBox(viewer, box, num, color, alpha);
}

struct Tester{

	Pose pose;
	bool init = true;
	int cycles = 0;
	pcl::console::TicToc timer;

	//thresholds
	double distThresh = 1e-3;
	double angleThresh = 1e-3;

	vector<double> distHistory;
	vector<double> angleHistory;

	void Reset(){
		cout << "Total time: " << timer.toc () << " ms, Total cycles: " << cycles << endl;
		init = true;
		cycles = 0;
		distHistory.clear();
		angleHistory.clear();
	}

	double angleMag( double angle){

		return abs(fmod(angle+pi, 2*pi) - pi);
	}

	bool Displacement( Pose p){

		if(init){
			timer.tic();
			pose = p;
			init = false;
			return true;
		}

		Pose movement = p - pose;
		double tdist = sqrt(movement.position.x * movement.position.x + movement.position.y * movement.position.y + movement.position.z * movement.position.z);
		double adist = max( max( angleMag(movement.rotation.yaw), angleMag(movement.rotation.pitch)), angleMag(movement.rotation.roll) );

		if(tdist > distThresh || adist > angleThresh){
			distHistory.push_back(tdist);
			angleHistory.push_back(adist);
			pose = p;

			cycles++;
			return true;
		}
		else
			return false;
	
	}

};

int main(){

	pcl::visualization::PCLVisualizer::Ptr viewer (new pcl::visualization::PCLVisualizer ("3D Viewer"));
  	viewer->setBackgroundColor (0, 0, 0);
	viewer->registerKeyboardCallback(keyboardEventOccurred, (void*)&viewer);
	viewer->setCameraPosition(pose.position.x, pose.position.y, 60, pose.position.x+1, pose.position.y+1, 0, 0, 0, 1);

	// Load map and display it
	PointCloudT::Ptr mapCloud(new PointCloudT);
  	pcl::io::loadPCDFile("map.pcd", *mapCloud);
  	cout << "Loaded " << mapCloud->points.size() << " data points from map.pcd" << endl;
	renderPointCloud(viewer, mapCloud, "map", Color(0,0,1)); 

	// True pose for the input scan
	vector<Pose> truePose ={Pose(Point(2.62296,0.0384164,0), Rotate(6.10189e-06,0,0)), Pose(Point(4.91308,0.0732088,0), Rotate(3.16001e-05,0,0))};
	drawCar(truePose[0], 0,  Color(1,0,0), 0.7, viewer);

	// Load input scan
	PointCloudT::Ptr scanCloud(new PointCloudT);
  	pcl::io::loadPCDFile("scan1.pcd", *scanCloud);
	
	typename pcl::PointCloud<PointT>::Ptr cloudFiltered (new pcl::PointCloud<PointT>);
	
	//vector<PointCloudT::Ptr> scans;
	//loadScans(scans, 1);
	// 
	//cloudFiltered = scanCloud; // TODO: remove this line
	//TODO: Create voxel filter for input scan and save to cloudFiltered
	pcl::VoxelGrid<PointT> vg;
	//vg.setInputCloud(scans[0]);
	vg.setInputCloud(scanCloud);
	vg.setLeafSize(0.5, 0.5, 0.5);
	vg.filter(*cloudFiltered);

	PointCloudT::Ptr transformed_scan (new PointCloudT);
	Tester tester;

	while (!viewer->wasStopped())
  	{
		Eigen::Matrix4d transform = transform3D(pose.rotation.yaw, pose.rotation.pitch, pose.rotation.roll, pose.position.x, pose.position.y, pose.position.z);

		if( matching != Off){
			if( matching == Icp)
				transform = ICP(mapCloud, cloudFiltered, pose, 3); //TODO: change the number of iterations to positive number
  			pose = getPose(transform);
			if( !tester.Displacement(pose) ){
				if(matching == Icp)
					cout << " Done testing ICP" << endl;
				tester.Reset();
				double pose_error = sqrt( (truePose[0].position.x - pose.position.x) * (truePose[0].position.x - pose.position.x) + (truePose[0].position.y - pose.position.y) * (truePose[0].position.y - pose.position.y) );
				cout << "pose error: " << pose_error << endl;
				matching = Off;
			}
		}
		
  		pcl::transformPointCloud (*cloudFiltered, *transformed_scan, transform);
		viewer->removePointCloud("scan");
		renderPointCloud(viewer, transformed_scan, "scan", Color(1,0,0)	);

		viewer->removeShape("box1");
		viewer->removeShape("boxFill1");
		drawCar(pose, 1,  Color(0,1,0), 0.35, viewer);
		
  		viewer->spinOnce ();
  	}

	return 0;
}
