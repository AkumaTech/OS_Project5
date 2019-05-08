/*
Project 5: Virtual Memory Manager
Miguel Hernandez
Miguel Manuel
Jose Avina
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define FILENAME 32
#define PAGE_TABLE_ENTRIES 256
#define PAGE_SIZE 256
#define TABLE_ENTRIES 16
#define FRAME_SIZE 256
#define FRAMES 128
#define MEMORY_SIZE (FRAME_SIZE*FRAMES)


struct fentry
{
	int fnum;
	struct fentry *next;
};

typedef struct fentry fflentry;
char *memory;
fflentry *frameList;

struct pageTblSNode
{
	int pnum;
	struct pageTblSNode *prev;
	struct pageTblSNode *next;
};

typedef struct pageTblSNode pStackNode;
pStackNode *ptop;
pStackNode *pbottom;

typedef struct 
{
	int pnum;
	int fnum;
} TabEntry;

typedef struct 
{
	int fnum;
	bool isValid;
	pStackNode *node;
} pageTableEntry;

pageTableEntry *pageTable;
TabEntry *Table;
int fTabIndex; 
int *tCounter;
int nRefer;
int nPageFaults;
int nTabHits;
int nCorrReads;

void getAddress(int add, int *pageNum, int *offset)
{
	int m = 0xff;
	*offset = add & m;
	*pageNum = (add >> 8) & m;
}

int getFreeFrame()
{
	fflentry *temp;
	int frameNum;
	pStackNode *pPtr;

	if(frameList == NULL)
	{
		pPtr = pbottom->prev;
		if(pPtr == ptop)
		{
			printf("error.\n");
			return 0;
		}
		pageTable[pPtr->pnum].isValid = false;
		pbottom->prev = pPtr->prev;
		pPtr->prev->next = pbottom;
	
		frameList = (fflentry *)malloc(sizeof(fflentry));
		frameList->fnum = pageTable[pPtr->pnum].fnum;
		frameList->next = NULL;

	}
	temp = frameList;
	frameList = temp->next;
	frameNum = temp->fnum;
	free(temp);
	
	return frameNum;
}

void pageTabInit()
{
	int i;
	pageTable = (pageTableEntry *)malloc(sizeof(pageTableEntry)*PAGE_TABLE_ENTRIES);
	Table = (TabEntry *)malloc(sizeof(pageTableEntry)*TABLE_ENTRIES);
	fTabIndex = 0;
	tCounter = (int *)malloc(sizeof(int)*TABLE_ENTRIES);

	ptop = (pStackNode *)malloc(sizeof(pStackNode));
	pbottom = (pStackNode *)malloc(sizeof(pStackNode));
	ptop->prev = NULL;
	pbottom->next = NULL;
	ptop->next = pbottom;
	pbottom->prev = ptop;

	for(i = 0; i < PAGE_TABLE_ENTRIES; ++i)
	{
		pageTable[i].isValid = false;
		pageTable[i].node = (pStackNode *)malloc(sizeof(pStackNode));
		pageTable[i].node->pnum = i;
	}
}

void memInit()
{
	int i, nFrame;
	fflentry *temp;
	memory = (char *)malloc(MEMORY_SIZE * sizeof(char));
	frameList = NULL;
	nFrame = MEMORY_SIZE/FRAME_SIZE;

	for(i = 0; i<nFrame; ++i)
	{
		temp = frameList;
		frameList = (fflentry *)malloc(sizeof(fflentry));
		frameList->fnum = nFrame - i - 1;
		frameList->next = temp;
	}
}
int translate(int lAdd, FILE *fBackingStore, int tCount)
{
	int pageNum, offset, frameNum, pAdd, i, TabI;
	int temp1, temp2;
	pStackNode *pPtr;

	++nRefer;
	getAddress(lAdd, &pageNum, &offset);

	for(i = 0; i<fTabIndex; ++i)
	{
		if(pageNum == Table[i].pnum)
		{
			++nTabHits;
			TabI = i;
			break;
		}
	}
	if(i == fTabIndex)
	{
		if(!pageTable[pageNum].isValid)
		{
			++nPageFaults;
			frameNum = getFreeFrame();
			fseek(fBackingStore, pageNum * PAGE_SIZE, SEEK_SET);
			fread(memory + frameNum * FRAME_SIZE, sizeof(char), FRAME_SIZE, fBackingStore);
			pageTable[pageNum].isValid = true;
			pageTable[pageNum].fnum = frameNum;
	
			pageTable[pageNum].node->next = ptop->next;
			ptop->next->prev = pageTable[pageNum].node;
			ptop->next = pageTable[pageNum].node;
			pageTable[pageNum].node->prev = ptop;		
		}
		frameNum = pageTable[pageNum].fnum;

		if(fTabIndex < TABLE_ENTRIES)
		{
			TabI = fTabIndex;
			++fTabIndex;
		}
		else
		{
			temp1 = tCounter[0];
			temp2 = 0;
			for(i = 0; i < TABLE_ENTRIES; ++i)
			{
				if(tCounter[i] < temp1)
				{
					temp1 = tCounter[i];
					temp2 = i;
				}
			}
			TabI = temp2;
		}
		Table[TabI].pnum = pageNum;
		Table[TabI].fnum = frameNum;
	}
	pPtr = pageTable[pageNum].node->next;
	pageTable[pageNum].node->next = ptop->next;
	ptop->next->prev = pageTable[pageNum].node;
	ptop->next = pageTable[pageNum].node;
	pPtr->prev = pageTable[pageNum].node->prev;
	pageTable[pageNum].node->prev->next = pPtr;
	pageTable[pageNum].node->prev = ptop;

	frameNum = Table[TabI].fnum;
	tCounter[TabI] = tCount;
	pAdd = (frameNum << 8) | offset;
	fseek(fBackingStore, 0, SEEK_SET);

	return pAdd;
}

char readMem(int pAdd)
{
	return memory[pAdd];
}

void pageTabClear()
{
	int i;
	for(i = 0; i < PAGE_TABLE_ENTRIES; ++i)
		free(pageTable[i].node);

	free(ptop);
	free(pbottom);
	free(pageTable);
	free(Table);
	free(tCounter);
}
void memClear()
{
	fflentry *temp;
	free(memory);
	while(frameList)
	{
		temp = frameList;
		frameList = temp->next;
		free(temp);
	}
}

int main (int argc, char *argv[])
{
	int lAdd, pAdd, cLAdd, cPAdd, cValue;
	FILE *fAdd, *fBackingStore, *fCorrect;
	int i;
	char content;

	pageTabInit();
	memInit();

	fBackingStore = fopen("BACKING_STORE.bin", "rb");
	fCorrect = fopen("correct.txt", "r");

	if(fBackingStore == NULL)
		{
			printf("no such file exist.\n");
			return 1;
		}
	if(fCorrect == NULL)
		{
			printf("no such file exist.\n");
			return 1;
		}
	nRefer = 0;
	nPageFaults = 0;
	nTabHits = 0;
	nCorrReads = 0;

	fAdd = fopen(argv[1], "r");
	if(fAdd == NULL)
		{
			printf("file was unable to open.\n");
			return 1;
		}		
	i = 0;
	while(~fscanf(fAdd, "%d", &lAdd))
		{
			pAdd = translate(lAdd, fBackingStore, i);
			content = readMem(pAdd);
			printf("Virtual Address: %d Physical Address: %d Value: %d\n", lAdd, pAdd, content);
			++i;
			fscanf(fCorrect, "Virtual Address: %d Physical Address: %d Value: %d\n", &cLAdd, &cPAdd, &cValue);
			if(lAdd != cLAdd)
				printf("Incorrect address.\n");
			if(cValue == (int) content)
				++nCorrReads;
		}

	printf("Initial Info======================\n");
	printf("Physical Memory size: %d\n", MEMORY_SIZE);
	printf("Virtual Memory size: %d\n", PAGE_TABLE_ENTRIES * PAGE_SIZE);
	printf("Number of frames: %d\n", FRAMES);
	printf("Number of page table entries: %d\n", PAGE_TABLE_ENTRIES);
	printf("These are the statistics==========\n");
	printf("Page fault rate: %f\n", (double) nPageFaults/ nRefer);
	printf("Table hit rate: %f\n", (double) nTabHits/ nRefer);
	printf("End of Info=======================\n");

	fclose(fAdd);
	fclose(fBackingStore);
	fclose(fCorrect);
	pageTabClear();
	memClear();

	return 0;
}
















