#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

#include "data-structs.h"

int sigFlag = 0;

void sigusr2_handler(int num) { // When SIGUSR2 signal is received, stop the loop
	sigFlag = 1;
}

int main(int argc, char *argv[]) {
	FILE *flog = NULL;

	ledgerRecord *bays;
	sharedMem *mem;

	int ttime = -1;
	int statTimes = -1;
	int shmid = -1;

	struct sigaction sa;
	sa.sa_handler = sigusr2_handler;
	sigaction(SIGUSR2, &sa, NULL);

	if (argc < 7) { // No arguments
		printf("Too few arguments.\nUsage: './comptroller -d time -t stattimes -s shmid'\n");

		return 1;
	}

	else if (argc > 7) { // Too many arguments
		printf("Too many arguments.\nUsage: './comptroller -d time -t stattimes -s shmid'\n");

		return 1;
	}

	else {
		for (int i = 1; i < argc; i += 2) { // Check all flags and parameters
			if (argv[i][0] == '-') { // If argument is flag
				switch (argv[i][1]) { // Check which option it is
					case 'd':
						if (ttime != -1) {
							printf("Duplicate flag: '%s'\nUsage: './comptroller -d time -t stattimes -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							ttime = atoi(argv[i+1]);

							if (ttime <= 0) {
								printf("Invalid parameter: time\n");

								return 2;
							}
						}

						break;

					case 't':
						if (statTimes != -1) {
							printf("Duplicate flag: '%s'\nUsage: './comptroller -d time -t stattimes -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							statTimes = atoi(argv[i+1]);

							if (statTimes <= 0) {
								printf("Invalid parameter: stattimes\n");

								return 2;
							}
						}

						break;

					case 's':
						if (shmid != -1) {
							printf("Duplicate flag: '%s'\nUsage: './comptroller -d time -t stattimes -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							shmid = atoi(argv[i+1]);

							if (shmid <= 0) {
								printf("Invalid parameter: shmid\n");

								return 2;
							}
						}

						break;

					default:
						printf("Invalid flag: '%s'\nUsage: './comptroller -d time -t stattimes -s shmid'\n", argv[i]);

						return 1;

				}
			}

			else {
				printf("Invalid argument: '%s'\nUsage: './comptroller -d time -t stattimes -s shmid'\n", argv[i]);

				return 1;
			}
		}
	}

	flog = fopen("log.txt", "a");
	if (flog == NULL) {
		printf("Invalid log file: File cannot be opened.\n");

		return 2;
	}
	fprintf(flog, "Comptroller: Process created.\n");
	fflush(flog);

	mem = (sharedMem *) shmat(shmid,(void *) 0 , 0);
	if (mem == (void *) -1 ) { // Check shmat failure
		perror("Comptroller shmat");

		return 3;
	}

	bays = (ledgerRecord *) ((sharedMem *) mem + 1); // Ledger records in all bays (first ASK bay, then PEL and lastly VOR)

	time_t prevTtime = 0;
	time_t prevStatTimes = 0;

	while (sigFlag == 0) {
		if (time(NULL) - prevTtime >= ttime) {
			sem_wait(&mem->ledgerAccess); // Start CS for accessing ledger

			int parkedBuses = 0;
			int currTotalPass = 0;
			for (int i = 0; i < 3 * (mem->bayCapacity); i++) {
				if (bays[i].statusFlag != 0) {

					currTotalPass += bays[i].numGetOff;
				}

				if (bays[i].statusFlag == 1) {

					parkedBuses += 1;
				}
			}
			printf("______________________________ STATION STATUS ______________________________\n");
			printf("Number of parked (sleeping) buses: %d\n", mem->numOfBuses[0] + mem->numOfBuses[1] + mem->numOfBuses[2]);

			printf("Number of empty (unassigned) parking spots: %d\n", (3 * mem->bayCapacity) - mem->numOfBuses[0] - mem->numOfBuses[1] - mem->numOfBuses[2]);
			printf("Number of empty (unassigned) parking spots in ASK bay: %d\n", mem->bayCapacity - mem->numOfBuses[0]);
			printf("Number of empty (unassigned) parking spots in PEL bay: %d\n", mem->bayCapacity - mem->numOfBuses[1]);
			printf("Number of empty (unassigned) parking spots in VOR bay: %d\n\n", mem->bayCapacity - mem->numOfBuses[2]);

			printf("Total passengers that got off from parked buses: %d\n", currTotalPass);
			
			// Detailed view of Ledger if needed for more details:

			// printf("________________ ASK BAY ________________\n");
			// for (int i = 0; i < 3 * (mem->bayCapacity); i++) {
			// 	if (i == mem->bayCapacity) {
			// 		printf("____________________________________________\n");
			// 		printf("\n________________ PEL BAY ________________\n");
			// 	}

			// 	else if (i == 2 * mem->bayCapacity) {
			// 		printf("____________________________________________\n");
			// 		printf("\n________________ VOR BAY ________________\n");
			// 	}
				
			// 	printf("Park. Spot: %d | ", i);
			// 	printf("Arr. Time: %ld | ", bays[i].arrivalTime);
			// 	printf("Lic. Plate: %s | ", bays[i].busLicencePlate);
			// 	printf("Type bus: %s | ", bays[i].busType);
			// 	printf("Type park. spot: %s | ", bays[i].parkType);
			// 	printf("Num. people off bus: %d | ", bays[i].numGetOff = 0);
			// 	printf("Status: %d\n", bays[i].statusFlag);
			// }
			// printf("____________________________________________\n");
			printf("____________________________________________________________________________\n");
			// printf("Status info: 0: Left / Empty, 1: Parked, 2: About to park, 3: About to leave\n");
			printf("\n\n\n\n\n");

			sem_post(&mem->ledgerAccess); // End CS for accessing ledger
			prevTtime = time(NULL);
		}

		if (time(NULL) - prevStatTimes >= statTimes) {
			sem_wait(&mem->statsAccess); // Start CS for accessing stats
			printf("____________________________ STATION STATISTICS ____________________________\n");
			printf("For passengers:\n");
			printf("Total passengers: %d\n", mem->stats.totalGetOn + mem->stats.totalGetOff);
			printf("Total passengers that got on buses so far: %d\n", mem->stats.totalGetOn);
			printf("Total passengers that got off buses so far: %d\n\n", mem->stats.totalGetOff);

			int totalBuses = mem->stats.totalBuses[0] + mem->stats.totalBuses[1] + mem->stats.totalBuses[2];
			printf("\nFor buses that had stayed and left:\n");
			printf("Total buses: %d\n", totalBuses);
			printf("Total ASK buses: %d\n", mem->stats.totalBuses[0]);
			printf("Total PEL buses: %d\n", mem->stats.totalBuses[1]);
			printf("Total VOR buses: %d\n", mem->stats.totalBuses[2]);
			if (totalBuses != 0) // Avoid division with 0
				printf("Average waiting time: %ld\n", (mem->stats.totalTimeWait[0] + mem->stats.totalTimeWait[1] + mem->stats.totalTimeWait[2]) / totalBuses);
			else
				printf("Average waiting time: 0\n");
			if (mem->stats.totalBuses[0] != 0) // Avoid division with 0
				printf("Average waiting time for ASK buses: %ld\n", mem->stats.totalTimeWait[0] / mem->stats.totalBuses[0]);
			else
				printf("Average waiting time for ASK buses: 0\n");
			if (mem->stats.totalBuses[1] != 0) // Avoid division with 0
				printf("Average waiting time for PEL buses: %ld\n", mem->stats.totalTimeWait[1] / mem->stats.totalBuses[1]);
			else
				printf("Average waiting time for PEL buses: 0\n");
			if (mem->stats.totalBuses[2] != 0) // Avoid division with 0
				printf("Average waiting time for VOR buses: %ld\n", mem->stats.totalTimeWait[1] / mem->stats.totalBuses[2]);
			else
				printf("Average waiting time for VOR buses: 0\n");
			if (totalBuses != 0) // Avoid division with 0
				printf("Average stay time: %ld\n", (mem->stats.totalTimeStay[0] + mem->stats.totalTimeStay[1] + mem->stats.totalTimeStay[2]) / totalBuses);
			else
				printf("Average stay time: 0\n");
			if (mem->stats.totalBuses[0] != 0)
				printf("Average stay time for ASK buses: %ld\n", mem->stats.totalTimeStay[0] / mem->stats.totalBuses[0]);
			else
				printf("Average stay time for ASK buses: 0\n");
			if (mem->stats.totalBuses[1] != 0)
				printf("Average stay time for PEL buses: %ld\n", mem->stats.totalTimeStay[1] / mem->stats.totalBuses[1]);
			else
				printf("Average stay time for PEL buses: 0\n");
			if (mem->stats.totalBuses[2] != 0)
				printf("Average stay time for VOR buses: %ld\n", mem->stats.totalTimeStay[1] / mem->stats.totalBuses[2]);
			else
				printf("Average stay time for VOR buses: 0\n");

			printf("____________________________________________________________________________\n");
			printf("\n\n\n\n\n");

			sem_post(&mem->statsAccess); // End CS for accessing stats
			prevStatTimes = time(NULL);
		}
	}

	if (shmdt((void *) mem) == -1) {
		perror ("Comptroller shmdt");

		return 4;
	}

	fprintf(flog, "Comptroller: Process finished.\n");
	fflush(flog);

	if (fclose(flog) == EOF) { // Check fclose failure
		printf("Error closing log file.\n");

		return 2;
	}
	flog = NULL;

	return 0;
}