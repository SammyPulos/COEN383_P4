#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define NUM_JOBS 500

int DEBUG_print_references;        // enables or disables refrence notifications
int DEBUG_trial_statistics[25][3]; // [Trial num][jobs, hits, misses]

typedef struct _page {
    int pageID;                // ID of the page, numbered 0 to (numberOfPages - 1) for each process
    int inMemory;              // 0 if page is not in memory
    int timePagedIn;           // time when the page was brought in from disk
    int timeLastReferenced;    // time the page was last referenced by the process
    int referenceCount;        // the number of times the page has been referenced
    int pid;                   // ID of the process the page belongs to
    struct _page * nextPtr;    // pointer to next page in the list
    int physPageID;            // ID of physical page in memory. corresponds to memID
} page;

typedef struct _memory {
    int busy;                  // 0 if the memory page is free
    page * pagePtr;            // pointer to page in memory
    struct _memory * nextPtr;  // pointer to next page of memory in the list
    int memID;                 // ID of physical page in memory
} memory;

typedef struct _process {
    int pid;                   // process ID
    float arrivalTime;         // time the process arrived at the queue is between 0.1s and 59.9s
    int serviceTime;           // time required to service the process
    int firstRunTime;          // time when the process starts
    int hitCount;              // number of page hits on this process' references
    int missCount;             // number of page misses on this process' references
    int numberOfPages;         // size of the process represented in pages randomly chosen as either 5, 11, 17, or 31
    page * pagePtr;            // pointer to head of its page list
    page * pageLastReferenced; // pointer to the page that the process has last referenced
    struct _process * nextPtr; // pointer to the next process in the list
} process;

typedef enum _policy {FIFO, LRU, LFU, MFU, RANDOM} policy;

// create the linked list of memory pages and return pointer to head node
memory * downloadRAM() {
     memory * rootPtr;
     memory * nodePtr;

     rootPtr = malloc(sizeof(memory));
     rootPtr->busy = 0;
     rootPtr->pagePtr = NULL;
     rootPtr->nextPtr = NULL;
     rootPtr->memID = 0;
     nodePtr = rootPtr;

     int i;
     for (i = 1; i < 100; ++i) {
          nodePtr->nextPtr = malloc(sizeof(memory));
          nodePtr = nodePtr->nextPtr;
          nodePtr->busy = 0;
          nodePtr->pagePtr = NULL;
          nodePtr->nextPtr = NULL;
          nodePtr->memID = i;
     }

     return rootPtr;
}

// randomly select the process size in pages
int randomNumberOfPages() {
     int r = rand() % 4;
     int numberOfPages;

     switch (r) {
          case 0:
               numberOfPages = 5;
               break;
          case 1:
               numberOfPages = 11;
               break;
          case 2:
               numberOfPages = 17;
               break;
          case 3:
               numberOfPages = 31;
               break;
          default:
               printf("HOW\n");
               exit(1);
     }

     return numberOfPages;
}

// Creates the linked list for the process's pages and returns a pointer to head of the list
page * generateProcessPageList(process * pageListOwner) {
     page * rootPtr;
     page * nodePtr;

     rootPtr = malloc(sizeof(page));
     rootPtr->pageID = 0;
     rootPtr->inMemory = 0;
     rootPtr->timePagedIn = -1;
     rootPtr->timeLastReferenced = -1;
     rootPtr->referenceCount = 0;
     rootPtr->pid = pageListOwner->pid;
     rootPtr->nextPtr = NULL;
     nodePtr = rootPtr;

     int i;
     for (i = 1; i < pageListOwner->numberOfPages; ++i) {
          nodePtr->nextPtr = malloc(sizeof(page));
          nodePtr = nodePtr->nextPtr;
          nodePtr->pageID = i;
          nodePtr->inMemory = 0;
          nodePtr->timePagedIn = -1;
          nodePtr->timeLastReferenced = -1;
          nodePtr->referenceCount = 0;
          nodePtr->pid = pageListOwner->pid;
          nodePtr->nextPtr = NULL;
     }

     return rootPtr;
}

// function that compares two values for qsort()
int compareArrivalTimes(const void * a, const void * b) {
     if (*(float *) a < *(float *) b)
          return -1;
     else if ( *(float *) a > *(float *) b)
          return 1;
     else
          return 0;
}

// generates and sorts the arrival times for the processes into an array
float * generateArrivalTimes() {
     float * arrivalTimes = malloc(sizeof(float) * NUM_JOBS);

     int i;
     for (i = 0; i < NUM_JOBS; ++i)
          arrivalTimes[i] = ((((float)rand() / (float)(RAND_MAX))) * 600) ;

     qsort(arrivalTimes, NUM_JOBS, sizeof(float), compareArrivalTimes);

     return arrivalTimes;
}

// generate ordered linked list of processes and return pointer to head node
process * generateProcesses() {
     process * rootPtr;
     process * nodePtr;

     float * sortedArrivalTimes = generateArrivalTimes();

     rootPtr = malloc(sizeof(process));
     rootPtr->pid = 0;
     rootPtr->arrivalTime = sortedArrivalTimes[0];
     rootPtr->serviceTime = ((rand() % 5) + 1) * 10;
     rootPtr->firstRunTime = -1;
     rootPtr->hitCount = 0;
     rootPtr->missCount = 0;
     rootPtr->numberOfPages = randomNumberOfPages();
     rootPtr->pagePtr = generateProcessPageList(rootPtr);
     rootPtr->pageLastReferenced = NULL;
     rootPtr->nextPtr = NULL;
     nodePtr = rootPtr;

     int i;
     for (i = 1; i < NUM_JOBS; ++i) {
          nodePtr->nextPtr = malloc(sizeof(process));
          nodePtr = nodePtr->nextPtr;
          nodePtr->pid = i;
          nodePtr->arrivalTime = sortedArrivalTimes[i];
          nodePtr->serviceTime = ((rand() % 5) + 1) * 10;
          nodePtr->firstRunTime = -1;
          nodePtr->hitCount = 0;
          nodePtr->missCount = 0;
          nodePtr->numberOfPages = randomNumberOfPages();
          nodePtr->pagePtr = generateProcessPageList(nodePtr);
          nodePtr->pageLastReferenced = NULL;
          nodePtr->nextPtr = NULL;
     }

     return rootPtr;
}

// returns total number of free pages in memory
int numberOfFreePages (memory * memPtr) {
     int freePages = 0;

     while (memPtr) {
          if (memPtr->busy == 0)
               ++freePages;
          memPtr = memPtr->nextPtr;
     }
     
     return freePages;
}

// returns the number of pages in memory that belong to the specified process
int numberOfProcessPagesInMemory(int pid, memory * memPtr) {
     int count = 0;
     while (memPtr) {
          if ((memPtr->busy) && (memPtr->pagePtr->pid == pid)) {
               ++count;
          }
          memPtr = memPtr->nextPtr;
     }
     return count;
}

// returns memory address of first free page in memory
memory * firstFreePage(memory * memPtr) {
     while (memPtr) {
          if (memPtr->busy == 0)
               return memPtr;
          memPtr = memPtr->nextPtr;
     }
     return memPtr;
}

memory * findPageInMemory(page * pagePtr, memory * memPtr) {
     while (memPtr) {
          if (memPtr->pagePtr && memPtr->pagePtr == pagePtr) {
               return memPtr;
          }
          memPtr = memPtr->nextPtr;
     }
     return memPtr;
}

// brings in a page from the disk to the memory to the first free page in memory
// returns 0 if page was already in memory, 1 otherwise
int pageIn(page * pagePtr, memory * memPtr, int time) {
     if (pagePtr->inMemory) {
          return 0;
     }

     memPtr = firstFreePage (memPtr);
     if(memPtr){
          memPtr->pagePtr = pagePtr;
          memPtr->busy = 1;
          pagePtr->inMemory = 1;
          pagePtr->timePagedIn = time;
          pagePtr->physPageID = memPtr->memID; //set the physical page ID
     }
     return 1;
}

// removes a page from memory
// returns 1 if page found and removed, 0 otherwise
int pageOut(page * pagePtr, memory * memPtr, int time) {
     while(memPtr) {
          if(memPtr->busy && memPtr->pagePtr == pagePtr) {
               // page found in memory
               memPtr->busy = 0;
               memPtr->pagePtr->inMemory = 0;
               memPtr->pagePtr->timeLastReferenced = 0;
               memPtr->pagePtr->referenceCount = 0;
               memPtr->pagePtr->physPageID = -1; //reset the physical page ID
               return 1;
          }
          memPtr = memPtr->nextPtr;
     }
     return 0;
}

// updates metadata for referencing a page
void referencePage(process * procPtr, page * pagePtr, memory * memPtr, int time) { 
     pagePtr->timeLastReferenced = time;
     pagePtr->referenceCount += 1;
     procPtr->pageLastReferenced = pagePtr;
}

// returns pointer of the most recently referenced page
page * previouslyReferencedPage(process * procPtr) {
     return procPtr->pageLastReferenced;
}

// generates a random page to reference
page * generateReference(process * procPtr, int time) {
     page * lastReferenced = previouslyReferencedPage(procPtr);
     int newReferencePageID = lastReferenced->pageID;
     int localityReferenceChance = rand() % 100;
     page * pageToReference = procPtr->pagePtr;

     if (localityReferenceChance <= 69) {
          int localityDifference = rand() % 3;
          switch (localityDifference) {
               case 0:
                    newReferencePageID = (lastReferenced->pageID + 1) % procPtr->numberOfPages;
                    break;
               case 1:
                    newReferencePageID = (lastReferenced->pageID - 1) % procPtr->numberOfPages;
                    if (newReferencePageID < 0)
                         newReferencePageID += procPtr->numberOfPages;
                    break;
               case 2:
                    break;
               default:
                    printf("ERROR GENERATING REFERENCE\n");
                    exit(1);
          }
     }

     else {
          int offsetBetweenPages = newReferencePageID - lastReferenced->pageID;
          while ((offsetBetweenPages < 2) && (offsetBetweenPages > -2)) {
               newReferencePageID = (lastReferenced->pageID + rand()) % procPtr->numberOfPages;
               offsetBetweenPages = newReferencePageID - lastReferenced->pageID;
          }
     }

     int i;
     for (i = 0; i < newReferencePageID; ++i)
          pageToReference = pageToReference->nextPtr;

     return pageToReference;
}

page * pageReplaceLRU(process * procPtr, memory * memPtr, int time, policy desiredPolicy) {
     page * LRU = NULL; 
     while (memPtr) {
          if (memPtr->busy && (memPtr->pagePtr->pid == procPtr->pid)) {
               if (!LRU) {
                    LRU = memPtr->pagePtr;
               } else if (memPtr->pagePtr->timeLastReferenced < LRU->timeLastReferenced) {
                    LRU = memPtr->pagePtr;
               }
          }
          memPtr = memPtr->nextPtr;
     }
     return LRU;
}

page * pageReplaceLFU(process * procPtr, memory * memPtr, int time, policy desiredPolicy) {
     page * LFU = NULL; 
     while (memPtr) {
          if (memPtr->busy && (memPtr->pagePtr->pid == procPtr->pid)) {
               if (!LFU) {
                    LFU = memPtr->pagePtr;
               } else if (memPtr->pagePtr->referenceCount < LFU->referenceCount) {
                    LFU = memPtr->pagePtr;
               }
          }
          memPtr = memPtr->nextPtr;
     }
     return LFU;
}

page * pageReplaceMFU(process * procPtr, memory * memPtr, int time, policy desiredPolicy) {
     page * MFU = NULL; 
     while (memPtr) {
          if (memPtr->busy && (memPtr->pagePtr->pid == procPtr->pid)) {
               if (!MFU) {
                    MFU = memPtr->pagePtr;
               } else if (memPtr->pagePtr->referenceCount > MFU->referenceCount) {
                    MFU = memPtr->pagePtr;
               }
          }
          memPtr = memPtr->nextPtr;
     }
     return MFU;
}

page * pageReplaceFIFO(process * procPtr, memory * memPtr, int time, policy desiredPolicy) {
     page * FIFO = NULL; 
     while (memPtr) {
          if (memPtr->busy && (memPtr->pagePtr->pid == procPtr->pid)) {
               if (!FIFO) {
                    FIFO = memPtr->pagePtr;
               } else if (memPtr->pagePtr->timePagedIn < FIFO->timePagedIn) {
                    FIFO = memPtr->pagePtr;
               }
          }
          memPtr = memPtr->nextPtr;
     }
     return FIFO;
}

page * pageReplaceRANDOM(process * procPtr, memory * memPtr, int time, policy desiredPolicy) {
     page * RANDOM = procPtr->pagePtr;
     int count = 0;

     while (RANDOM) {
          if (RANDOM->inMemory)
               ++count;
          RANDOM = RANDOM->nextPtr;
     }

     RANDOM = procPtr->pagePtr;
     int randomCount = (rand() % count) + 1;

     while (RANDOM) {
          if (RANDOM->inMemory == 1) {
               randomCount -= 1;
               if (randomCount == 0)
                    break;
          }
          RANDOM = RANDOM->nextPtr;
     }

     return RANDOM;
}

page * pageReplace(process * procPtr, memory * memPtr, int time, policy desiredPolicy) {
     page * pageToReplace = NULL;
     switch (desiredPolicy) {
          case LRU:
               pageToReplace = pageReplaceLRU(procPtr, memPtr, time, desiredPolicy);
               break;
          case LFU:
               pageToReplace = pageReplaceLFU(procPtr, memPtr, time, desiredPolicy);
               break;
          case MFU:
               pageToReplace = pageReplaceMFU(procPtr, memPtr, time, desiredPolicy);
               break;
          case FIFO:
               pageToReplace = pageReplaceFIFO(procPtr, memPtr, time, desiredPolicy);
               break;
          case RANDOM:
               pageToReplace = pageReplaceRANDOM(procPtr, memPtr, time, desiredPolicy);
               break;
          default:
               printf("ERROR INVALID POLICY\n");
               exit(1);
     }
     return(pageToReplace);
}

// checks if the process has started running by the specified time
int processStarted(process * procPtr, int time) {
     return ((procPtr->firstRunTime != -1) && (procPtr->firstRunTime <= time));
}

// checks if the process is complete
int processCompleted(process * procPtr, int time) {
     return (time >= (procPtr->firstRunTime + procPtr->serviceTime));
}

void printMemoryMap(memory * memPtr) {
     printf("Memory map\n");
     int counter = 0;
     int entriesPerLine = 20;
     while (memPtr) {
          if (memPtr->busy) {
               printf("%03d ", memPtr->pagePtr->pid);
          }
          else {
               printf("... ");
          }          
          memPtr = memPtr->nextPtr;

          if (counter == entriesPerLine - 1)
               printf("\n");
          counter = ((counter + 1) % entriesPerLine);
     }

     printf("\n");
}

// initiates process by referencing their page 0 
void startProcess(process * procPtr, memory * memPtr, int time) {
     printf("\nSWAPPING time: %d.%03d, proc: %3d, Enter, size: %2d, service: %d seconds\n", 
          (time*100)/1000, (time*100)%1000, procPtr->pid, procPtr->numberOfPages, procPtr->serviceTime/10);
     printMemoryMap(memPtr);
     procPtr->firstRunTime = time;
     pageIn(procPtr->pagePtr, memPtr, time);
     referencePage(procPtr, procPtr->pagePtr, memPtr, time);
     procPtr->missCount += 1;
     if (DEBUG_print_references) {
          printf("REF MISS time: %d.%03d, proc: %3d, ref page: %2d, frame: %2d\n",
               (time*100)/1000, (time*100)%1000, procPtr->pid, procPtr->pagePtr->pageID, procPtr->pagePtr->physPageID);
     }
}

// stops process and removes its pages from memory, returns the number of pages removed
int stopProcess(process * procPtr, memory * memPtr, int time) { 
     int count = 0;
     memory * memHead = memPtr;
     while(memPtr) {
          if(memPtr->pagePtr && (memPtr->pagePtr->pid == procPtr->pid)) {
               memPtr->busy = 0;
               memPtr->pagePtr->inMemory = 0;
               ++count;
          }
          memPtr = memPtr->nextPtr;
     }

     printf("\nSWAPPING time: %d.%03d, proc: %3d,  Exit, size: %2d, service: %d seconds\n", 
          (time*100)/1000, (time*100)%1000, procPtr->pid, procPtr->numberOfPages, procPtr->serviceTime/10);
     printMemoryMap(memHead);

     return count;
}

// brings in jobs that have arrived into memory if there is room for them
void bringInWaitingJobs(process * procPtr, memory * memPtr, int time) {
     while (procPtr) {
          if ((procPtr->arrivalTime < (float)time) && (procPtr->firstRunTime == -1) && (numberOfFreePages(memPtr) >= 4)){
               startProcess(procPtr, memPtr, time);
          }
          procPtr = procPtr->nextPtr;
     }
}

// takes all jobs that are have run for their service time out of memory
void kickOutCompletedJobs(process * procPtr, memory * memPtr, int time) {
     while (procPtr) {
          if(processStarted(procPtr, time) && processCompleted(procPtr, time) && 
               !processCompleted(procPtr, time-1)) {  
               stopProcess(procPtr, memPtr, time);
          }
          procPtr = procPtr->nextPtr;
     }
}
          
int numberOfJobsRun(process * procPtr) {
     int counter = 0;
     while(procPtr) {
          if(procPtr->firstRunTime != -1) {
               ++counter;
          }
          procPtr = procPtr->nextPtr;
     }
     return counter;
}

int totalNumberOfHits(process * procPtr) {
     int counter = 0;
     while(procPtr) {
          counter += procPtr->hitCount;
          procPtr = procPtr->nextPtr;
     }
     return counter;
}

int totalNumberOfMisses(process * procPtr) {
     int counter = 0;
     while(procPtr) {
          counter += procPtr->missCount;
          procPtr = procPtr->nextPtr;
     }
     return counter;
}

int main() {
     //srand(time(NULL));
     DEBUG_print_references = 1;
     srand(1337);
     memory * memHead = downloadRAM();
     process * procHead = generateProcesses();

     int time;
     policy desiredPolicy = MFU;

     for (time = 0; time < 600; ++time) {
          process * procPtr = procHead;

          kickOutCompletedJobs(procHead, memHead, time);
          bringInWaitingJobs(procHead, memHead, time);

          procPtr = procHead;
          while (procPtr) {
               if (processStarted(procPtr, time - 1) && !processCompleted(procPtr, time)) {
                    //generate desired page
                    page * desiredPage = generateReference(procPtr, time);
                    //if desired page is not in memory
                    if (desiredPage->inMemory == 0) {
                         if (numberOfFreePages(memHead) <= 0) {
                              // MISS needs replacement
                              procPtr->missCount += 1;
                              page * pageToRemove = pageReplace(procPtr, memHead, time, desiredPolicy);
                              pageOut(pageToRemove, memHead, time);
                              pageIn(desiredPage, memHead, time);
                              if (DEBUG_print_references) {
                                   printf("REF MISS time: %d.%03d, proc: %3d, ref page: %2d, frame: %2d, page out: %2d\n",
                                        (time*100)/1000, (time*100)%1000, procPtr->pid, desiredPage->pageID, 
                                        desiredPage->physPageID, pageToRemove->pageID);
                              }
                         }
                         else {
                              // MISS with free pages
                              procPtr->missCount += 1;
                              pageIn(desiredPage, memHead, time);
                              if (DEBUG_print_references) {
                                   printf("REF MISS time: %d.%03d, proc: %3d, ref page: %2d, frame: %2d\n",
                                        (time*100)/1000, (time*100)%1000, procPtr->pid, desiredPage->pageID, 
                                        desiredPage->physPageID);
                              }
                         }
                    }
                    else {
                         // HIT
                         procPtr->hitCount += 1;
                         if (DEBUG_print_references) {
                              printf("REF HIT  time: %d.%03d, proc: %3d, ref page: %2d, frame: %2d\n",
                                   (time*100)/1000, (time*100)%1000, procPtr->pid, desiredPage->pageID, 
                                   desiredPage->physPageID);     
                         }
                    }
                    //actually reference the page
                    referencePage(procPtr, desiredPage, memHead, time);
               }
               procPtr = procPtr->nextPtr;
          } 
     }

     DEBUG_trial_statistics[0][0] = numberOfJobsRun(procHead);
     DEBUG_trial_statistics[0][1] = totalNumberOfHits(procHead);
     DEBUG_trial_statistics[0][2] = totalNumberOfMisses(procHead);

     printf("\nTrial 0 stastics: number of jobs run = %d, hit/miss ratio = %f  \n", 
          DEBUG_trial_statistics[0][0], (((float)DEBUG_trial_statistics[0][1]) / ((float)DEBUG_trial_statistics[0][2])));

     return 0;
}
