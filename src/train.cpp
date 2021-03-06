/** Copyright 2013. All rights reserved.
* Author: German Ros (gros@cvc.uab.es)
*         Advanced Driver Assistance Systems (ADAS)
*         Computer Vision Center (CVC)
*	  Universitat Autònoma de Barcelona (UAB)
*
* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or any later version.
*
* This software is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
* PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* this software; if not, write to the Free Software Foundation, Inc., 51 Franklin
* Street, Fifth Floor, Boston, MA 02110-1301, USA 
*
*/

#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>

#include <pcl/io/pcd_io.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include "yass.hpp"
#include "CurvatureWrapper.hpp"
#include "PositionWrapper.hpp"
#include "ColorNWrapper.hpp"
#include "DONWrapper.hpp"
#include "FPFHWrapper.hpp"

using namespace std;
using namespace pcl;

struct mRGB
{
	int r, g, b;
	
	mRGB(int r, int g, int b) : r(r), g(g), b(b) {}
};

vector<string> readDir(string directory)
{
        vector<string> result;
        struct dirent **namelist;
        int n,i;

        n = scandir(directory.c_str(), &namelist, 0, versionsort);
        if (n < 0)
                cerr << "[readDir]: Error, no files found" << endl;
        else
        {
                for(i =0 ; i < n; ++i)
                {
                        if(namelist[i]->d_type == DT_REG)
                        {
                                //cout << namelist[i]->d_name << endl;
                                result.push_back(namelist[i]->d_name);
                                free(namelist[i]);
                        }
                }
                free(namelist);
        }

        return result;
}

void savingLabels(vector<int>& labels, string outputFile)
{
	ofstream fd(outputFile.c_str());
	
	for(int l=0; l<labels.size(); l++)
	{
		fd << labels[l] << endl;
	}

	fd.close();
}

void generateSemanticPointCloud(PointCloud<pcl::PointXYZRGB>::Ptr& cloud, vector<int>& labels, vector<mRGB>& colorScheme)
{
	for(int p=0; p<cloud->size(); p++)
	{
		mRGB c = colorScheme.at(labels[p]);
		cloud->points[p].r = c.r;
		cloud->points[p].g = c.g;
		cloud->points[p].b = c.b;
	}
}

int main(int argc, char** argv)
{
	int NLabels = 3;

	if(argc != 3)
	{
		cerr << "[main] Syntax error. Try with: " << argv[0] << " path_to_training_pointclouds modelName" << endl;
		return -1;
	}

	string trainingDir = string(argv[1]);
	string modelName = string(argv[2]);

	// reading training files (pointclouds as PointXYZRGBL
	vector<string> trainingFiles = readDir(trainingDir);	
	// loading clouds
	vector<pcl::PointCloud<pcl::PointXYZRGBL>::Ptr> trainingClouds;
	for(int i=0; i<trainingFiles.size(); i++)
	{
		pcl::PointCloud<pcl::PointXYZRGBL>::Ptr tempCloud (new pcl::PointCloud<pcl::PointXYZRGBL>);
		int result = pcl::io::loadPCDFile<pcl::PointXYZRGBL> (trainingDir + string("/") + trainingFiles[i], *tempCloud);
		if(result != -1)
			trainingClouds.push_back(tempCloud);
	}

	cout << "\t** [Loaded " << trainingClouds.size() << " training clouds]" << endl;

	//let's configure the features we want to use
	FeatureManager fManager;

	// FPFH...
	FPFHWrapper* fFPFH = new FPFHWrapper(0.1, 0.2);
	// curvatures...
	CurvatureWrapper* fCurvature = new CurvatureWrapper(0.1, 0.3);
	// 3D Position
	PositionWrapper* fPosition = new PositionWrapper();
	// Normalized colour
	ColorNWrapper* fColorN = new ColorNWrapper();
	// DoN...
	DONWrapper* fDON = new DONWrapper(0.1, 0.2);

	fManager.addFeatureExtractor(fFPFH);
	fManager.addFeatureExtractor(fCurvature);
	fManager.addFeatureExtractor(fPosition);
	fManager.addFeatureExtractor(fColorN);
	fManager.addFeatureExtractor(fDON);

	// set up liblinear options
	double weights[] = {1.0, 1.0, 1.0};
	int weight_labels[] = {0, 1, 2};
	struct parameter paramsLiblinear;
	paramsLiblinear.solver_type = L1R_L2LOSS_SVC; //MCSVM_CS;
	paramsLiblinear.eps = 0.01;
	paramsLiblinear.C = 1.0;
	paramsLiblinear.nr_weight = NLabels;
	paramsLiblinear.weight = weights;
	paramsLiblinear.weight_label = weight_labels;
	paramsLiblinear.p = 0.1;

	//train model
	YASS3D yass(fManager, NLabels);
	yass.setLiblinearOptions(&paramsLiblinear, true, false);
	model* mymodel = yass.trainModel(trainingClouds);
	
	save_model(modelName.c_str(), mymodel); 

	//release memory
	//free_and_destroy_model(&mymodel);
	//destroy_param(&paramsLiblinear);

	return 0;
}
