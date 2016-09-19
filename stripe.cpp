#include "stripe.h"

void Stripe::ComputeOffsets()
{
	if (isRowMajor){
		colOffset=0;
		rowOffset=stripeIndex*stripeSize;
	}
	else{
		rowOffset=0;
		colOffset=stripeIndex*stripeSize;	
		}
}
float Stripe::asFloat(int row, int col)
{
	return pDem->asFloat(rowOffset+row, colOffset+col);
}

int Stripe::is_NoData(int row, int col)
{
	return pDem->is_NoData(rowOffset+row, colOffset+col);
}
int Stripe::is_InDEM(int row, int col){
	return pDem->is_InGrid(rowOffset+row, colOffset+col);
}
int Stripe::is_InStrip(int row, int col)
{
	if (row>=0 && row<height && col>=0 && col<width) return true;
	return false;
}

void  Stripe::SetData(int row, int col,float val)
{
	pDem->Set_Value(rowOffset+row, colOffset+col,val);
}
int Stripe::isCellOnInterStripBorder(int row, int col,int& cellIndex,int& borderIndex, float& spill)
{
	if (isRowMajor) {
		if (row==height-1) {	
			borderIndex=2;
			cellIndex=col;
			spill=border2.arrBorderNodes[cellIndex].spill;
			return true;	
		}
		else if (row==0)
		{
			borderIndex=1;
			cellIndex=col;
			spill=border1.arrBorderNodes[cellIndex].spill;
			return true;
		}
		return false;
	}
	else {
		if (col==width-1) {
			cellIndex=row;	
			borderIndex=2;
			spill=border2.arrBorderNodes[cellIndex].spill;
			return true;	
		}
		else if (col==0)
		{
			cellIndex=row;
			borderIndex=1;
			spill=border1.arrBorderNodes[cellIndex].spill;
			return true;
		}
		return false;
	}	
}
//for depression border cells
void Stripe::AddCellToBorderDCList(int cellIndex,int borderIndex,float spill)
{
	if (borderIndex==1){
		border1.arrSpillChangedNodeIndex.push_back(cellIndex);
		border1.arrBorderNodes[cellIndex].spill=spill;
		border1.arrBorderNodes[cellIndex].isDepressionCell=true;
	}
	else
	{
		border2.arrSpillChangedNodeIndex.push_back(cellIndex);
		border2.arrBorderNodes[cellIndex].spill=spill;
		border2.arrBorderNodes[cellIndex].isDepressionCell=true;
	}
}

void Stripe::Initialize()
{
	ComputeOffsets();
	mainFlag.Init(width,height);
	depressionFlag.Init(width,height);
	int size,i;
	if (isRowMajor)
		size=width;
	else
		size=height;

	//allocate space for border cell arrays
	border1.Init(size);
	border2.Init(size);	
	borderInNeighborStrip1.Init(size);
	borderInNeighborStrip2.Init(size);	
	//
	if (isRowMajor){
		if (stripeIndex>0) {
			for(i=0;i<size; i++) 
				border1.arrBorderNodes[i].spill=asFloat(0,i);
		}
		if (stripeIndex<stripeNumber-1){
			for(i=0;i<size; i++) 
				border2.arrBorderNodes[i].spill=asFloat(height-1,i);
		}
	}
	else {
		if (stripeIndex>0) {
			for(i=0;i<size; i++) 
				border1.arrBorderNodes[i].spill=asFloat(i,0);
		}
		if (stripeIndex<stripeNumber-1){
			for(i=0;i<size; i++) 
				border2.arrBorderNodes[i].spill=asFloat(i,width-1);
		}
	}
}

void Stripe::PushBorderCellsIntoPQ()
{
	Node tmpNode;
	int row, col,iRow, iCol,i;
	//Push real border cells of original DEM to PQ
	for ( row = 0; row <height; row++)
	{
		for (col = 0; col <width; col++)
		{
			//is the cell a NoData cell
			if (!is_NoData(row, col))
			{
				//iterate its eight neighbors
				for (i = 0; i < 8; i++)
				{
					iRow = Get_rowTo(i, row);
					iCol = Get_colTo(i, col);
					//if its neighbor is outside DEM (not this stripe) or NODATA cell
					//it is on the real border of original DEM 	
					if (!is_InDEM(iRow, iCol) || is_NoData(iRow, iCol))
					{
						//push into PQ
						tmpNode.col = col;
						tmpNode.row = row;
						tmpNode.spill = asFloat(row, col);
						priorityQueue.push(tmpNode);
						mainFlag.SetFlag(row,col);
						break;
					}
				}
			}
			else{
				mainFlag.SetFlag(row,col);
			}
		}
	}
}

void Stripe::ProcessSlopeCells(int isFirstTime)
{
	int iRow, iCol,i;
	float iSpill,val;
	int cellIndex, borderIndex;
	Node N,node,headNode;
	int total=0,nPSC=0;
	bool bInPQ=false;
	bool isBoundary;
	int j,jRow,jCol;
	while (!traceQueue.empty())
	{
		node = traceQueue.front();
		traceQueue.pop();
		bInPQ=false;
		if (!isFirstTime) { //by default, all border cells are slope cells during the first time
			if (isCellOnInterStripBorder(node.row, node.col,cellIndex,borderIndex,val)){
				AddSlopeCellToListWithLowerSpillEle(cellIndex,borderIndex,node.spill);
			}
		}
 		for (i = 0; i < 8; i++)
		{
			iRow = Get_rowTo(i, node.row);
			iCol = Get_colTo(i, node.col);
			if (mainFlag.IsProcessed(iRow,iCol) || depressionFlag.IsProcessed(iRow,iCol)) continue;		
			
			iSpill =asFloat(iRow, iCol);
			
			if (iSpill <= node.spill) 	{
				if (!bInPQ) {
					//decide whether (iRow, iCol) is a real slope border cell
					//whetehr i can flow to a flagged j
					isBoundary=true;
					for (j = 0; j < 8; j++)
					{
						jRow = Get_rowTo(j, iRow);
						jCol = Get_colTo(j, iCol);
						if (!is_InStrip(jRow,jCol))continue; //this is different from the global method beause this case does not occur in global method;
						if ((mainFlag.IsProcessedDirect(jRow,jCol)|| depressionFlag.IsProcessedDirect(jRow,jCol)) && asFloat(jRow,jCol)<iSpill)
						//i flows to j; i is not a slope border cell
						{
							isBoundary=false;
							break;
						}
					}
					if (isBoundary) {
						priorityQueue.push(node);
						bInPQ=true;
					}
				}
				continue;  //i is lower than current node; current node is not pushed to the tracequeue
			}

			//N is unflagged and N>C
			//N is puhsed to traceque
			N.col = iCol;
			N.row = iRow;
			N.spill = iSpill;
			traceQueue.push(N);
			mainFlag.SetFlag(iRow,iCol); //set processed flag		
		}
	}
}

void Stripe::ProcessDepressionCells()
{
	int iRow, iCol,i;
	float iSpill,val;
	Node N;
	Node node,frontNode;
	int borderDCNum=0; //border depression cell number
	int borderIndex,cellIndex;
	frontNode=depressionQue.front();
	while (!depressionQue.empty())
	{
		node= depressionQue.front();
		depressionQue.pop();
		//process strip border cells in the depression
		if (isCellOnInterStripBorder(node.row, node.col,cellIndex,borderIndex,val)){
			borderDCNum++;
			AddCellToBorderDCList(cellIndex,borderIndex,node.spill);
		}
		for (i = 0; i < 8; i++)
		{
			iRow = Get_rowTo(i, node.row);
			iCol = Get_colTo(i,  node.col);
			if (!is_InStrip(iRow,iCol))continue; 
			if (depressionFlag.IsProcessedDirect(iRow,iCol) || mainFlag.IsProcessedDirect(iRow,iCol)) continue;		
			iSpill = asFloat(iRow, iCol);
			N.row = iRow;
			N.col = iCol;
			if (iSpill > node.spill) 
			{  //not in the depression, should be pushed to the traceQ
				mainFlag.SetFlag(iRow,iCol);
				N.spill = iSpill;				
				traceQueue.push(N);
				continue;
			}
			
			//lower than spill cell and is a depression cell
			//mark the cell
			depressionFlag.SetFlag(iRow,iCol);
			N.spill = node.spill;
			depressionQue.push(N);
		}
	}
	
	if (borderDCNum>0) return;

	//Otherwise,the final spill cell has been found, fill the depression
	FillDepression(frontNode);	
}
void Stripe::FillDepression(Node node)
{
	//depressionQue should be empty now
	int row, col,iRow, iCol,i;
	float iSpill;
	Node N;
	depressionQue.push(node);
	mainFlag.SetFlag(node.row, node.col);
	while (!depressionQue.empty())
	{
		node= depressionQue.front();
		depressionQue.pop();
		SetData(node.row, node.col,node.spill);
		//depressionFlag.RemoveFlag(node.row, node.col);			
		for (i = 0; i < 8; i++)
		{
			iRow = Get_rowTo(i, node.row);
			iCol = Get_colTo(i,  node.col);

			if (mainFlag.IsProcessed(iRow,iCol)) continue;		
			mainFlag.SetFlag(iRow,iCol);
			N.row = iRow;
			N.col = iCol;
			N.spill=node.spill;
			iSpill=asFloat(iRow,iCol);
			if (iSpill<=node.spill) {
				depressionQue.push(N);
			}
		}
	}
}

void Stripe::PriorityFlood(int isFirstTime)
{
	Node node, N;
	int iRow, iCol,i,row, col;
	int cellIndex,borderIndex;
	float iSpill, spill,val;
	if (!isFirstTime) { //this initialization is done  during the first time by default
		depressionFlag.ClearAllFlags();
		border1.arrSpillChangedNodeIndex.clear();
		border2.arrSpillChangedNodeIndex.clear();
	}
	while (!priorityQueue.empty()){
		node=priorityQueue.top();
		priorityQueue.pop();
		for (i = 0; i < 8; i++)
		{
			iRow = Get_rowTo(i, node.row);
			iCol = Get_colTo(i, node.col);

			//check both flags
			if (!is_InStrip(iRow,iCol))continue; 
			if (!isFirstTime) {
				if (isCellOnInterStripBorder(iRow,iCol,cellIndex,borderIndex,val)){
					if (val<node.spill) continue;//no need to process iCell because iCell's spill is lower than node.spill
												 //this happens when a neighboring border cell is pushed neighbored by two border cells	
				}
			}
			if (mainFlag.IsProcessedDirect(iRow,iCol) || depressionFlag.IsProcessedDirect(iRow,iCol)) 
				continue;		
			iSpill = asFloat(iRow, iCol);
			N.row = iRow;
			N.col = iCol;
			if (iSpill <= node.spill) 
			{  
				// it is a depression cell 
				depressionFlag.SetFlag(iRow,iCol);
				N.spill = node.spill;				
				depressionQue.push(N);
				ProcessDepressionCells();
			}	
			else {
				//it is a slope cell, mask it in the main mask
				mainFlag.SetFlag(iRow,iCol);
				N.spill = iSpill;
				traceQueue.push(N);
			}
			ProcessSlopeCells(isFirstTime);
		}
	}	
}

int Stripe::PushCellIntoPQ_FirstTime(StripeBorder& border, StripeBorder& borderInNeighboringStrip, int borderIndex)
{
	unsigned int index;
	float spill,ispill;
	int i;
	int findLowerSpillElevation;
	int row,col;
	int total=0;
	int offset=0;

	for (index=0;index<border.arrSpillChangedNodeIndex.size(); index++){
		i=border.arrSpillChangedNodeIndex[index];
		spill=border.arrBorderNodes[i].spill;
		findLowerSpillElevation=false;
		if (i>0) {
			ispill=borderInNeighboringStrip.arrBorderNodes[i-1].spill;
			if (fabs(NO_DATA_VALUE-ispill)>0.001) {
				if (ispill<spill) {
					spill=ispill;
					findLowerSpillElevation=true;
					offset=-1;
				}
			}
		}
		ispill=borderInNeighboringStrip.arrBorderNodes[i].spill;
		if (fabs(NO_DATA_VALUE-ispill)>0.001) {
			if (ispill<spill) {
				spill=ispill;
				findLowerSpillElevation=true;
				offset=0;
			}
		}

		if (i+1<border.arrBorderNodes.size()) {
			ispill=borderInNeighboringStrip.arrBorderNodes[i+1].spill;
			if (fabs(NO_DATA_VALUE-ispill)>0.001) {
				if (ispill<spill) {
					spill=ispill;
					findLowerSpillElevation=true;
					offset=1;
				}
			}
		}

		if (findLowerSpillElevation){
			if (borderInNeighboringStrip.arrBorderNodes[i+offset].isPushedToPQAlready) continue;
			borderInNeighboringStrip.arrBorderNodes[i+offset].isPushedToPQAlready=true;
			if (isRowMajor) {
				col=i+offset;
				if (borderIndex==1)
				{
					row=-1;
				}
				else {
					row=height;
				}
			}
			else {
				row=i+offset;
				if (borderIndex==1)
				{
					col=-1;
				}
				else {
					col=width;
				}
			}
			priorityQueue.push(Node(row,col,spill));
			total++;
		}
	}
	return total;
}
int Stripe::UpdatePotentialSpillandPushToPQ_FirstTime(Stripe* pStrip)
{
	int total=0;
	if(stripeIndex>0) {
		borderInNeighborStrip1.CopyFrom(pStrip[stripeIndex-1].border2);
		total+=PushCellIntoPQ_FirstTime(border1,borderInNeighborStrip1,1);
	}

	if (stripeIndex<stripeNumber-1){
		borderInNeighborStrip2.CopyFrom(pStrip[stripeIndex+1].border1);
		total+=PushCellIntoPQ_FirstTime(border2,borderInNeighborStrip2,2);
	} 
	return total;
}

//the following section process cells at other times
/*****************************************************************************************/


void Stripe::AddSlopeCellToListWithLowerSpillEle(int cellIndex,int borderIndex,float spill)
{
	if (borderIndex==1){
		//changes made
		border1.arrSpillChangedNodeIndex.push_back(cellIndex);
		border1.arrBorderNodes[cellIndex].spill=spill;
		border1.arrBorderNodes[cellIndex].isDepressionCell=false;
	}
	else
	{
		border2.arrSpillChangedNodeIndex.push_back(cellIndex);
		border2.arrBorderNodes[cellIndex].spill=spill;
		border2.arrBorderNodes[cellIndex].isDepressionCell=false;
	}
}

int Stripe::UpdatePotentialSpillandPushToPQ_OtherTimes(Stripe* pStrip)
{
	int total=0;
	if(stripeIndex>0) {
		borderInNeighborStrip1.CopyFrom(pStrip[stripeIndex-1].border2);
		total+=PushCellIntoPQ_OtherTimes(border1,borderInNeighborStrip1,1);
	}

	if (stripeIndex<stripeNumber-1){
		borderInNeighborStrip2.CopyFrom(pStrip[stripeIndex+1].border1);
		total+=PushCellIntoPQ_OtherTimes(border2,borderInNeighborStrip2,2);
	} 
	return total;
}

int Stripe::PushCellIntoPQ_OtherTimes(StripeBorder& border, StripeBorder& borderInNeighboringStrip, int borderIndex)
{
	unsigned int index;
	float spill,ispill;
	int i;
	int findDCWithHigherSpillElevation;
	int row,col;
	int total=0;
	int offset=0;
	for (index=0;index<borderInNeighboringStrip.arrSpillChangedNodeIndex.size(); index++){
		i=borderInNeighboringStrip.arrSpillChangedNodeIndex[index];
		spill=borderInNeighboringStrip.arrBorderNodes[i].spill; //find the changed cell in neighboring cell
		findDCWithHigherSpillElevation=false;
		
		if (i>0) {
			if (border.arrBorderNodes[i-1].isDepressionCell) {
				ispill=border.arrBorderNodes[i-1].spill;
				if (fabs(NO_DATA_VALUE-ispill)>0.001) {
					if (spill<ispill) {
						findDCWithHigherSpillElevation=true;
//						offset=1;
					}
				}
			}
		}
		if (!findDCWithHigherSpillElevation) {
			if (border.arrBorderNodes[i].isDepressionCell) {
				ispill=border.arrBorderNodes[i].spill;
				if (fabs(NO_DATA_VALUE-ispill)>0.001) {
					if (spill<ispill) {
						findDCWithHigherSpillElevation=true;
//						offset=0;
					}
				}
			}
		}
		if (!findDCWithHigherSpillElevation) {
			if (border.arrBorderNodes[i+1].isDepressionCell) {
				if (i+1<border.arrBorderNodes.size()) {
					ispill=border.arrBorderNodes[i+1].spill;
					if (fabs(NO_DATA_VALUE-ispill)>0.001) {
						if (spill<ispill) {
							findDCWithHigherSpillElevation=true;
//							offset=-1;
						}
					}
				}
			}
		}

		if (findDCWithHigherSpillElevation){
			if (isRowMajor) {
				col=i+offset;
				if (borderIndex==1)
				{
					row=-1;
				}
				else {
					row=height;
				}
			}
			else {
				row=i+offset;
				if (borderIndex==1)
				{
					col=-1;
				}
				else {
					col=width;
				}
			}
			priorityQueue.push(Node(row,col,spill));
			total++;
		}
	}
	return total;
}

void Stripe::FillDepressionFromStripBorder(){
	int size,i;
	if (isRowMajor)
		size=width;
	else
		size=height;

	if (isRowMajor) {
		if (stripeIndex>0) {
			for (i=0; i<size; i++){
				if (border1.arrBorderNodes[i].isDepressionCell && !mainFlag.IsProcessed(0,i)){
					FillDepression(Node(0,i,border1.arrBorderNodes[i].spill));
				}
			} 
		}
		if (stripeIndex<stripeNumber-1) {
			for (i=0; i<size; i++){
				if (border2.arrBorderNodes[i].isDepressionCell && !mainFlag.IsProcessed(height-1,i)){
					FillDepression(Node(height-1,i,border2.arrBorderNodes[i].spill));
				}
			} 
		}
	}
	else {
		if (stripeIndex>0) {
			for (i=0; i<size; i++){
				if (border1.arrBorderNodes[i].isDepressionCell && !mainFlag.IsProcessed(i,0)){
					FillDepression(Node(i,0,border1.arrBorderNodes[i].spill));
				}
			} 
		}
		if (stripeIndex<stripeNumber-1) {
			for (i=0; i<size; i++){
				if (border2.arrBorderNodes[i].isDepressionCell && !mainFlag.IsProcessed(i,width-1)){
					FillDepression(Node(i,width-1,border2.arrBorderNodes[i].spill));
				}
			} 
		}
	}
}