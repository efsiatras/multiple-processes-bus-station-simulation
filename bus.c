#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "data-structs.h"

int typeToIndex(char *type);

int main(int argc, char *argv[]) {
	FILE *flog = NULL;

	char *licencePlate = NULL;
	char *type = NULL;
	int incPassengers = -1;
	int capacity = -1;
	int parkPeriod = -1;
	int manTime = -1;
	int shmid = -1;

	sharedMem *mem;
	if (argc < 15) { // No arguments
		printf("Too few arguments.\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n");

		return 1;
	}

	else if (argc > 15) { // Too many arguments
		printf("Too many arguments.\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n");

		return 1;
	}

	else {
		for (int i = 1; i < argc; i += 2) { // Check all flags and parameters
			if (argv[i][0] == '-') { // If argument is flag
				switch (argv[i][1]) { // Check which option it is
					case 'l':
						if (licencePlate != NULL) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							licencePlate = argv[i+1];

							if (strlen(licencePlate) > 8) {
								printf("Invalid parameter: licenceplate\n");

								return 2;
							}
						}

						break;

					case 't':
						if (type != NULL) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							type = argv[i+1];

							if ((!strcmp(type, "ASK")) && (!strcmp(type, "PEL")) && (!strcmp(type, "VOR"))) {
								printf("Invalid parameter: type\n");

								return 2;
							}
						}

						break;

					case 'n':
						if (incPassengers != -1) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							incPassengers = atoi(argv[i+1]);

							if (incPassengers <= 0) {
								printf("Invalid parameter: incpassengers\n");

								return 2;
							}
						}

						break;

					case 'c':
						if (capacity != -1) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							capacity = atoi(argv[i+1]);

							if (capacity <= 0) {
								printf("Invalid parameter: capacity\n");

								return 2;
							}
						}

						break;

					case 'p':
						if (parkPeriod != -1) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							parkPeriod = atoi(argv[i+1]);

							if (parkPeriod <= 0) {
								printf("Invalid parameter: parkperiod\n");

								return 2;
							}
						}

						break;

					case 'm':
						if (manTime != -1) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
							return 1;
						}

						else {
							manTime = atoi(argv[i+1]);

							if (manTime <= 0) {
								printf("Invalid parameter: mantime\n");

								return 2;
							}
						}

						break;

					case 's':
						if (shmid != -1) {
							printf("Duplicate flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);
							
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
						printf("Invalid flag: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);

						return 1;

				}
			}

			else {
				printf("Invalid argument: '%s'\nUsage: './bus -l licenceplate -t type -n incpassengers -c capacity -p parkperiod -m mantime -s shmid'\n", argv[i]);

				return 1;
			}
		}
	}

	mem = (sharedMem *) shmat(shmid,(void *) 0 , 0);
	if (mem == (void *) -1 ) { // Check shmat failure
		perror("Bus shmat");

		return 3;
	}

	if (incPassengers > mem->maxNumPassengers) {  // Check bus parameters and log file parameters 
		printf("Invalid parameter: incpassengers\n");
		shmdt((void *) mem);

		return 2;
	}

	if (capacity > mem->maxNumPassengers) {  // Check bus parameters and log file parameters 
		printf("Invalid parameter: capacity\n");
		shmdt((void *) mem);

		return 2;
	}

	if (parkPeriod > mem->maxParkPeriod) { // Check bus parameters and log file parameters 
		printf("Invalid parameter: parkperiod\n");
		shmdt((void *) mem);

		return 2;
	}

	flog = fopen("log.txt", "a");
	if (flog == NULL) {
		printf("Invalid log file: File cannot be opened.\n");

		return 2;
	}

	srand(time(NULL));
	time_t arrivalTime = time(NULL) - mem->initTime;

	// ----------------------------------------------------------------------
	int responseFlag;
	do {
		sem_wait(&mem->requestAccess); // Start CS for filling in request

		mem->request.busArrivalTime = arrivalTime;
		strcpy(mem->request.busLicencePlate, licencePlate);
		strcpy(mem->request.busType, type);
		mem->request.typeFlag = 1; // Request for entering
		mem->request.busNumGetOff = incPassengers;

		sem_post(&mem->requestNotif); // Notify of request
		sem_wait(&mem->responseNotif); // Wait for response
		responseFlag = mem->request.responseFlag;

		sem_post(&mem->requestAccess); // End CS for filling in request

		if (responseFlag == 0) {
			sem_wait(&mem->countBusesWaitAccess); // Start CS for updating countBusesWait
			(mem->countBusesWait)++;
			sem_post(&mem->countBusesWaitAccess); // End CS for updating countBusesWait

			sem_wait(&mem->spotEmptiedNotif); // FIFO Queue for waiting buses ; Wait for notification when spot emptied
		}

	} while (responseFlag == 0);
	// ----------------------------------------------------------------------

	// ----------------------------------------------------------------------
	sem_wait(&mem->enterMan); // Wait for space for manoeuvring (incoming buses)

	time_t waitTime = time(NULL) - mem->initTime - arrivalTime;

	sleep(manTime); // Sleep manoeuvre time

	sem_wait(&mem->requestAccess); // Start CS for filling in request

	strcpy(mem->request.busLicencePlate, licencePlate);
	mem->request.typeFlag = 3; // Request to inform manoeuvre is over

	sem_post(&mem->requestNotif); // Notify of request
	sem_wait(&mem->responseNotif); // Wait for response

	sem_post(&mem->requestAccess); // End CS for filling in request

	sem_post(&mem->enterMan); // Not manoeuvring anymore, parked
	// ----------------------------------------------------------------------
	sem_wait(&mem->statsAccess); // Start CS for accessing stats
	mem->stats.totalGetOff += incPassengers;
	sem_post(&mem->statsAccess); // End CS for accessing stats

	sleep(parkPeriod); // Sleep park period
	int numGetOn = 1 + rand() % capacity; // [1, capacity]

	sem_wait(&mem->statsAccess); // Start CS for accessing stats
	mem->stats.totalGetOn += numGetOn;
	sem_post(&mem->statsAccess); // End CS for accessing stats

	// ----------------------------------------------------------------------
	sem_wait(&mem->requestAccess); // Start CS for filling in request

	strcpy(mem->request.busLicencePlate, licencePlate);
	mem->request.typeFlag = 2; // Request for leaving

	sem_post(&mem->requestNotif); // Notify of request
	sem_wait(&mem->responseNotif); // Wait for response
	responseFlag = mem->request.responseFlag;

	sem_post(&mem->requestAccess); // End CS for filling in request
	// ----------------------------------------------------------------------

	// ----------------------------------------------------------------------
	sem_wait(&mem->leaveMan); // Wait for space for manoeuvring (outgoing buses)

	sleep(manTime); // Sleep manoeuvre time

	sem_wait(&mem->requestAccess); // Start CS for filling in request

	strcpy(mem->request.busLicencePlate, licencePlate);
	mem->request.typeFlag = 4; // Request to inform manoeuvre is over

	sem_post(&mem->requestNotif); // Notify of request
	sem_wait(&mem->responseNotif); // Wait for response

	sem_post(&mem->requestAccess); // End CS for filling in request

	sem_post(&mem->leaveMan); // Not manoeuvring anymore, left

	time_t stayTime = time(NULL) - mem->initTime - waitTime;
	sem_wait(&mem->statsAccess); // Start CS for accessing stats
	(mem->stats.totalBuses[typeToIndex(type)])++;
	mem->stats.totalTimeWait[typeToIndex(type)] += waitTime;
	mem->stats.totalTimeStay[typeToIndex(type)] += stayTime;
	sem_post(&mem->statsAccess); // End CS for accessing stats
	// ----------------------------------------------------------------------

	if (shmdt((void *) mem) == -1) {
		perror ("Bus shmdt");

		return 4;
	}

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