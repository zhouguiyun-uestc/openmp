#include "stripe.h"
#include <omp.h>
Stripe* CreateStrips(int height, int width, int stripNumber,CDEM* pDEM)
{
	int i;
	int isRowMajor;
	int linePerStrip;
	Stripe* pStrips=new Stripe[stripNumber];
	int total;
	if (height>=width) {
		isRowMajor=true;
		total=height;
	}
	else { 
		isRowMajor=false;
		total=width;
	}

	linePerStrip=total/stripNumber;	
	for (i=0; i<stripNumber; i++){
		pStrips[i].pDem=pDEM;
		pStrips[i].stripeIndex=i;
		pStrips[i].stripeNumber=stripNumber;
		pStrips[i].isRowMajor=isRowMajor;
		pStrips[i].stripeSize=linePerStrip;
		if (isRowMajor)
		{
			pStrips[i].width=width;
			pStrips[i].height=linePerStrip;
			if (i==stripNumber-1)
				pStrips[i].height=total-i*linePerStrip;
		}
		else
		{
			pStrips[i].height=height;
			pStrips[i].width=linePerStrip;
			if (i==stripNumber-1)
				pStrips[i].width=total-i*linePerStrip;
		}

	}

	return pStrips;
}

void FillDEM_Parallel(char* inputFile, char* outputFilledPath,int threadNumber)
{
	CDEM dem;
	double geoTransformArgs[6];
	std::cout<<"Reading tiff files..."<<endl;
	if (!readTIFF(inputFile, GDALDataType::GDT_Float32, dem, geoTransformArgs))
	{
		printf("read tif error!\n");
		return;
	}

	queue<Node> traceQueue;//
	queue<Node> depressionQue;//
	
	std::cout<<"Finish reading data"<<endl;

	time_t timeStart, timeEnd;
	int width = dem.Get_NX();
	int height = dem.Get_NY();
	
	timeStart = time(NULL);
	std::cout<<"Using "<< threadNumber << " threads to fill DEM"<<endl;

	//Create Strips DEM
	Stripe* pStrips=CreateStrips(height,width,threadNumber,&dem); 
	int i;

	omp_set_dynamic(0);     // Explicitly disable dynamic teams
	omp_set_num_threads(threadNumber); //Specify the number of threads for all consecutive parallel regions

	//Process strips first time
	#pragma omp parallel for
	for (i=0;i<threadNumber;i++){
		pStrips[i].Initialize();
		pStrips[i].PushBorderCellsIntoPQ();
		pStrips[i].PriorityFlood(true);
	}

	int total=0;
	#pragma omp parallel for reduction (+:total)
	for (i=0;i<threadNumber;i++){
		total+=pStrips[i].UpdatePotentialSpillandPushToPQ_FirstTime(pStrips);
	}
	
	//process strips at other times
	while (true){
		if (total==0) break;

		#pragma omp parallel for
		for (i=0;i<threadNumber;i++){
			pStrips[i].PriorityFlood(false);
		}

		total=0;
		#pragma omp parallel for reduction (+:total)
		for (i=0;i<threadNumber;i++){
			total+=pStrips[i].UpdatePotentialSpillandPushToPQ_OtherTimes(pStrips);
		}
	}

	// fill remaining depressions
	#pragma omp parallel for
	for (i=0;i<threadNumber;i++){
		pStrips[i].FillDepressionFromStripBorder();
	}


	delete[] pStrips;

	timeEnd = time(NULL);
	double consumeTime = difftime(timeEnd, timeStart);
	std::cout<<"Time used:"<<consumeTime<<" seconds"<<endl;

	//compare file
	//output filled depression
	double min, max, mean, stdDev;
	calculateStatistics(dem, &min, &max, &mean, &stdDev);
	CreateGeoTIFF(outputFilledPath, dem.Get_NY(), dem.Get_NX(), 
		(void *)dem.getDEMdata(),GDALDataType::GDT_Float32, geoTransformArgs,
		&min, &max, &mean, &stdDev, -9999);
	return;
}
