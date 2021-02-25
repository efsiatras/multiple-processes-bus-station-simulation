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

int typeToIndex(char *type);
void indexToType(int ind, char *type);

int main(int argc, char *argv[]) {
	setvbuf(stdout, NULL, _IOLBF, 0);
	struct timespec ts;
	FILE *flog = NULL;

	ledgerRecord *bays;
	sharedMem *mem;

	int shmid = -1;
	int maxInactive = -1;
	int inactiveFlag = 0;

	if (argc < 5) { // No arguments
		printf("Too few arguments.\nUsage: './station-manager -i maxInactive -s shmid'\n");

		return 1;
	}

	else if (argc > 5) { // Too many arguments
		printf("Too many arguments.\nUsage: './station-manager -i maxInactive -s shmid'\n");

		return 1;
	}

	else {
		for (int i = 1; i < argc; i += 2) { // Check all flags and parameters
			if (argv[i][0] == '-') { // If argument is flag
				switch (argv[i][1]) { // Check which option it is
					case 'i':
						maxInactive = atoi(argv[i+1]);

						if (maxInactive <= 0) {
							printf("Invalid parameter: maxInactive\n");

							return 2;
						}

						break;

					case 's':
						shmid = atoi(argv[i+1]);

						if (shmid <= 0) {
							printf("Invalid parameter: shmid\n");

							return 2;
						}

						break;

					default:
						printf("Invalid flag: '%s'\nUsage: './station-manager -i maxInactive -s shmid'\n", argv[i]);

						return 1;

				}
			}

			else {
				printf("Invalid argument: '%s'\nUsage: './station-manager -i maxInactive -s shmid'\n", argv[i]);

				return 1;
			}
		}
	}

	flog = fopen("log.txt", "a");
	if (flog == NULL) {
		printf("Invalid log file: File cannot be opened.\n");

		return 2;
	}
	fprintf(flog, "Station-manager: Process created.\n");
	fflush(flog);

	mem = (sharedMem *) shmat(shmid,(void *) 0 , 0);
	if (mem == (void *) -1 ) { // Check shmat failure
		perror("Station-manager shmat");

		return 3;
	}

	bays = (ledgerRecord *) ((sharedMem *) mem + 1);
	srand(time(NULL));

	while (1) {
		clock_gettime(CLOCK_REALTIME, &ts);
		ts.tv_sec += maxInactive;
		inactiveFlag = sem_timedwait(&mem->requestNotif, &ts);
		if (inactiveFlag != 0) {
			fprintf(flog, "Station-manager: Maximum inactive time exceeded.\n");
			fflush(flog);

			kill(getppid(),SIGUSR2); // Send SIGUSR2 Signal to mystation

			break;
		}
		switch (mem->request.typeFlag) { // Check which type of request it is
			// ----------------------------------------------------------------------
			case 1: // 1: Request for entering
				sem_wait(&mem->ledgerAccess); // Start CS for accessing ledger

				char parkType[4]; // Bay with empty spot to be filled (following the bus types' parking rules)
				int typeInd = typeToIndex(mem->request.busType);
				if (typeInd == 0) { // ASK type
					if (mem->numOfBuses[typeInd] < mem->bayCapacity) { // If there is empty spot in ASK bay
						strcpy(parkType, "ASK");
					}

					else if (mem->numOfBuses[1] < mem->bayCapacity) { // If there is empty spot in PEL bay
						strcpy(parkType, "PEL");
					}

					else {
						strcpy(parkType, "");
					}
				}

				else if (typeInd == 1) { // PEL type
					if (mem->numOfBuses[typeInd] < mem->bayCapacity) { // If there is empty spot in PEL bay
						strcpy(parkType, "PEL");
					}

					else {
						strcpy(parkType, "");
					}
				}

				else if (typeInd == 2) { // VOR type
					if (mem->numOfBuses[typeInd] < mem->bayCapacity) { // If there is empty spot in VOR bay
						strcpy(parkType, "VOR");
					}

					else if (mem->numOfBuses[1] < mem->bayCapacity) { // If there is empty spot in PEL bay
						strcpy(parkType, "PEL");
					}

					else {
						strcpy(parkType, "");
					}
				}

				else {
					strcpy(parkType, "");
				}

				sem_post(&mem->ledgerAccess); // End CS for accessing ledger

				if (strcmp(parkType, "")) { // If empty spot was found
					sem_wait(&mem->ledgerAccess);  // Start CS for accessing ledger
					int parkInd = typeToIndex(parkType); // Bay to park index
					for (int i = 0; i < mem->bayCapacity; i++) {
						if (bays[parkInd * mem->bayCapacity + i].statusFlag == 0) { // Empty spot found again

							bays[parkInd * mem->bayCapacity + i].arrivalTime = mem->request.busArrivalTime;
							strcpy(bays[parkInd * mem->bayCapacity + i].busLicencePlate, mem->request.busLicencePlate);
							strcpy(bays[parkInd * mem->bayCapacity + i].busType, mem->request.busType);
							strcpy(bays[parkInd * mem->bayCapacity + i].parkType, parkType);
							bays[parkInd * mem->bayCapacity + i].numGetOff = mem->request.busNumGetOff;
							bays[parkInd * mem->bayCapacity + i].statusFlag = 2; // 'About to park'

							(mem->numOfBuses[parkInd])++;
							fprintf(flog, "Station-manager: Parking spot reserved for %s (type %s) in %s bay.\n", mem->request.busLicencePlate, mem->request.busType, parkType);
							fflush(flog);

							break;
						}
					}
					sem_post(&mem->ledgerAccess); // End CS for accessing ledger

					mem->request.responseFlag = 1;
				}

				else { // No empty spot found, tell bus to sleep
					fprintf(flog, "Station-manager: No parking spot found for %s (type %s).\n", mem->request.busLicencePlate, mem->request.busType);
					fflush(flog);

					mem->request.responseFlag = 0;
				}

				break;

			// ----------------------------------------------------------------------
			case 2: // 2: Request for leaving
				sem_wait(&mem->ledgerAccess); // Start CS for accessing ledger

				for (int i = 0; i < 3 * (mem->bayCapacity); i++) {
					if ((bays[i].statusFlag == 1) && !strcmp(bays[i].busLicencePlate, mem->request.busLicencePlate)) { // Bus found 'Parked'
						bays[i].statusFlag = 3; // 'About to leave'
						fprintf(flog, "Station-manager: %s (type %s) wants to leave from %s bay.\n", bays[i].busLicencePlate, bays[i].busType, bays[i].parkType);
						fflush(flog);

						break;
					}
				}

				sem_post(&mem->ledgerAccess); // End CS for accessing ledger

				break;

			// ----------------------------------------------------------------------
			case 3: // 3: Request to inform manoeuvre is over (update from 'About to park' to 'Parked')
				sem_wait(&mem->ledgerAccess); // Start CS for accessing ledger

				for (int i = 0; i < 3 * (mem->bayCapacity); i++) {
					if ((bays[i].statusFlag == 2) && !strcmp(bays[i].busLicencePlate, mem->request.busLicencePlate)) { // Bus found 'About to park'
						bays[i].statusFlag = 1; // 'Parked'
						fprintf(flog, "Station-manager: Confirmed %s (type %s) finished manoeuvring and parked in %s bay.\n", bays[i].busLicencePlate, bays[i].busType, bays[i].parkType);
						fflush(flog);

						break;
					}
				}

				sem_post(&mem->ledgerAccess); // End CS for accessing ledger

				break;

			// ----------------------------------------------------------------------
			case 4: // 4: Request to inform manoeuvre is over (update from 'About to leave' to 'Left')
				sem_wait(&mem->ledgerAccess); // Start CS for accessing ledger

				for (int i = 0; i < 3 * (mem->bayCapacity); i++) {
					if ((bays[i].statusFlag == 3) && !strcmp(bays[i].busLicencePlate, mem->request.busLicencePlate)) { // Bus found 'About to leave'
						bays[i].statusFlag = 0; // 'Left'
						fprintf(flog, "Station-manager: Confirmed %s (type %s) finished manoeuvring and left the station.\n", bays[i].busLicencePlate, bays[i].busType);
						fflush(flog);

						(mem->numOfBuses[i / (mem->bayCapacity)])--;

						break;
					}
				}

				sem_post(&mem->ledgerAccess); // End CS for accessing ledger

				sem_wait(&mem->countBusesWaitAccess); // Start CS for updating countBusesWait
				for (int i = 0; i < mem->countBusesWait; i++) { // For all waiting buses
					sem_post(&mem->spotEmptiedNotif); // Notify of spot emptied
				}

				mem->countBusesWait = 0;
				sem_post(&mem->countBusesWaitAccess); // End CS for updating countBusesWait

				break;
		}

		sem_post(&mem->responseNotif);
	}

	if (shmdt((void *) mem) == -1) {
		perror ("Station-manager shmdt");

		return 4;
	}

	fprintf(flog, "Station-manager: Process finished.\n");
	fflush(flog);

	if (fclose(flog) == EOF) { // Check fclose failure
		printf("Error closing log file.\n");

		return 2;
	}
	flog = NULL;

	return 0;
}

int typeToIndex(char *type) {
	if (!strcmp(type, "ASK")) {

		return 0;
	}

	else if (!strcmp(type, "PEL")) {

		return 1;
	}

	else if (!strcmp(type, "VOR")) {

		return 2;
	}

	return -1;
}

void indexToType(int ind, char *type) {
	if (ind == 0) {

		strcpy(type, "ASK");
	}

	else if (ind == 1) {

		strcpy(type, "PEL");
	}

	else if (ind == 2) {

		strcpy(type, "VOR");
	}
}