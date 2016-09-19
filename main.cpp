#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <queue>
#include <algorithm>
#include "dem.h"
#include "node.h"
#include "utils.h"
#include <time.h>
#include <list>
#include <unordered_map>

#ifdef WIN32
#include "DebugDiag.h"
#endif

using namespace std;
using std::cout;
using std::endl;
using std::string;
using std::getline;
using std::fstream;
using std::ifstream;
using std::priority_queue;
using std::binary_function;


typedef std::vector<Node> NodeVector;
typedef std::priority_queue<Node, NodeVector, Node::Greater> PriorityQueue;

void FillDEM_Parallel(char* inputFile, char* outputFilledPath,int threadNumber);
void FillDEM_Zhou_OnePass(char* inputFile, char* outputFilledPath);
//compute stats for a DEM
void calculateStatistics(const CDEM& dem, double* min, double* max, double* mean, double* stdDev)
{
	int width = dem.Get_NX();
	int height = dem.Get_NY();

	int validElements = 0;
	double minValue, maxValue;
	double sum = 0.0;
	double sumSqurVal = 0.0;
	for (int row = 0; row < height; row++)
	{
		for (int col = 0; col < width; col++)
		{
			if (!dem.is_NoData(row, col))
			{
				double value = dem.asFloat(row, col);
				
				if (validElements == 0)
				{
					minValue = maxValue = value;
				}
				validElements++;
				if (minValue > value)
				{
					minValue = value;
				}
				if (maxValue < value)
				{
					maxValue = value;
				}

				sum += value;
				sumSqurVal += (value * value);
			}
		}
	}

	double meanValue = sum / validElements;
	double stdDevValue = sqrt((sumSqurVal / validElements) - (meanValue * meanValue));
	*min = minValue;
	*max = maxValue;
	*mean = meanValue;
	*stdDev = stdDevValue;
}



CDEM* diff(CDEM& demA, CDEM& demB)
{
	CDEM* pRestultDEM = new CDEM();

	int widthA = demA.Get_NX();
	int heightA = demA.Get_NY();
	int widthB = demB.Get_NX();
	int heightB = demB.Get_NY();

	
	if (widthA != widthB || heightA != heightB)
	{
		cout<<"Dimensions of DEM are different£¡"<<endl;
		return NULL;
	}

	pRestultDEM->SetHeight(heightA);
	pRestultDEM->SetWidth(widthA);
	if (!pRestultDEM->Allocate())
	{
		cout<<"Not enough memory!"<<endl;
		return NULL;
	}

	float valueA, valueB;
	for (int row = 0; row < heightA; row++)
	{
		for (int col = 0; col < widthA; col++)
		{
			if (demA.is_InGrid(row, col) && !demA.is_NoData(row, col) 
				&& demB.is_InGrid(row, col) && !demB.is_NoData(row, col))
			{
				valueA = demA.asFloat(row, col);
				valueB = demB.asFloat(row, col);

				float tmp = valueA - valueB;
				pRestultDEM->Set_Value(row, col, tmp);
			}

		}
	}
	return pRestultDEM;
}

int CompareDEMs(const char *demA, const char *demB)
{
	GDALDataType type=GDALDataType::GDT_Float32;
	CDEM originDEM;
	double originDEMgeoTransformEles[6];
	if (!readTIFF(demA, type, originDEM, originDEMgeoTransformEles))
	{
		cout<<"Failed to read tiff file!"<<endl;
		return true;
	}
	CDEM DEMarc;
	double DEMarcgeoTransformEles[6];

	if (!readTIFF(demB, type, DEMarc, DEMarcgeoTransformEles))
	{
		cout<<"Failed to read tiff file!"<<endl;
		return true;
	}

	CDEM* result = diff(originDEM, DEMarc);
	double min, max, mean, stdDev;
	calculateStatistics(*result, &min, &max, &mean, &stdDev);
	if (fabs(min-max)>0.00001f) {
		printf("\n**********************************************\n*************************************************\nDEMs are different. min=%lf, max=%lf\n",min,max);
		return false;
	}
	else {
		printf("\nThe two DEMs are identical.\n");
	}
	delete result;
	return true;
}
int main(int argc, char* argv[])
{
	if (argc < 4)
	{
		cout<<"\nopenmpfill usage: openmpfill processMethod inputfileName outputfileName "<<endl;
		cout<<"\nThe operations conduceted depend on the value of 'processMehtod'£º"<<endl;
		cout<<"zhou-onepass: using the one-pass implementation in Zhou et al.(2006)"<<endl;
		cout<<"pl: using the parallel implementation in the manuscript. You should also provide the number of threads as last argument"<<endl;
		cout<<"diff: compare two DEMs"<<endl;

		cout<<"\nExample usage:"<<endl;
		cout<<"\nopenmpfill zhou-onepass dem.tif dem_filled.tif"<<endl;		
		cout<<"\nopenmpfill pl dem.tif dem_filled_pl.tif  4"<<endl;
		cout<<"\nopenmpfill diff dem.tif dem_filled_pl.tif  "<<endl;

		return 1;
	}
	
	string path(argv[2]);
	string outputFilledPath(argv[3]);
	size_t index = path.find(".tif");
	if (index ==string::npos) {
		cout<<"Input file name should have an extension of '.tif'"<<endl;
		return 1;
	}
	char* method=argv[1];
	string strFolder = path.substr(0, index);
	if (strcmp(method,"zhou-onepass")==0)
	{
		FillDEM_Zhou_OnePass(&path[0], &outputFilledPath[0]); //zhou
	}
	else if (strcmp(method,"pl")==0)
	{
		if (argc<5) {
			cout<<"\nWhen using the parallel implementation, you should provide the number of threads as the last argument."<<endl;
			cout<<"\nFor example: openmpfill pl dem.tif dem_filled.tif 4"<<endl;

			return 1;
		}
		int num=-1;
		sscanf(argv[4],"%d",&num);
		if (num<1) {
			cout<<"\nThe number of threads is invalid."<<endl;
			return 1;
		}
		FillDEM_Parallel(&path[0], &outputFilledPath[0],num); //zhou
	}
	else if (strcmp(method,"diff")==0)
	{
		index = path.find_last_of("\\");
		strFolder = path.substr(0, index);
		string outputFilledPath = strFolder + "\\diff.tif";
		CompareDEMs(argv[2],argv[3]);
	}
	else 
	{
		cout<<"Unknown process method"<<endl;
	}
	return 0;
}

