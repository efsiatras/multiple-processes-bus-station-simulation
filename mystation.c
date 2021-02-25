#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "data-structs.h"

#define SEGMENTPERM 0666
#define SMALLBUFF 10

void sigusr2_handler(int num) {}

int main(int argc, char *argv[]) {
	FILE *flog = NULL;
	char *configFile = NULL;
	FILE *fconfig = NULL;

	struct sigaction sa;
	sa.sa_handler = sigusr2_handler;
	sigaction(SIGUSR2, &sa, NULL);

	if (argc < 3) { // No arguments
		printf("Too many arguments.\nUsage: './mystation -l configfile'\n");

		return 1;
	}

	else if (argc > 3) { // Too many arguments
		printf("Too many arguments.\nUsage: './mystation -l configfile'\n");

		return 1;
	}

	else {
		for (int i = 1; i < argc; i += 2) { // Check all flags and parameters
			if (argv[i][0] == '-') { // If argument is flag
				switch (argv[i][1]) { // Check which option it is
					case 'l':
						configFile = argv[i+1];

						fconfig = fopen(configFile, "r");
						if (fconfig == NULL) {
							printf("Invalid config file: File does not exist.\n");

							return 2;
						}

						break;

					default:
						printf("Invalid flag: '%s'\nUsage: './mystation -l configfile'\n", argv[i]);

						return 1;

				}
			}

			else {
				printf("Invalid argument: '%s'\nUsage: './mystation -l configfile'\n", argv[i]);

				return 1;
			}
		}
	}
	time_t initTime = time(NULL);

	int numOfBays = -1;
	int bayCapacity = -1;
	int maxNumPassengers = -1;
	int maxParkPeriod = -1;
	int numCreatedBuses = -1;
	int maxInactive = -1;

	fscanf(fconfig,"%d", &numOfBays);
	fscanf(fconfig,"%d", &bayCapacity);
	fscanf(fconfig,"%d", &maxNumPassengers);
	fscanf(fconfig,"%d", &maxParkPeriod);
	fscanf(fconfig,"%d", &numCreatedBuses);
	fscanf(fconfig, "%d", &maxInactive);

	if (numOfBays <= 0 || bayCapacity <= 0 || maxNumPassengers <= 0 || maxParkPeriod <= 0 || numCreatedBuses <= 0 || maxInactive <= 0) {
		printf("Invalid parameters in config file.\n");

		return 1;
	}

	if (fclose(fconfig) == EOF) { // Check fclose failure
		printf("Error closing config file.\n");

		return 2;
	}
	fconfig = NULL;
	configFile = NULL;

	flog = fopen("log.txt", "a");
	if (flog == NULL) {
		printf("Invalid log file: File cannot be opened.\n");

		return 2;
	}

	int shmId = shmget(IPC_PRIVATE, sizeof(sharedMem) + numOfBays * bayCapacity * sizeof(ledgerRecord), SEGMENTPERM);
	if (shmId == -1) {
        perror("Mystation shmget\n");

		return 3;
	}
	char shmIdBuff[SMALLBUFF];
	sprintf(shmIdBuff, "%d", shmId); // ShmId to array of chars

	sharedMem *mem = (sharedMem *) shmat(shmId,(void *) 0 , 0);
	if (mem == (void *) -1) { // Check shmat failure
		perror("Bus shmat");

		return 4;
	}

	int retval = 0;
	retval += sem_init(&mem->requestAccess, 1, 1); // Only one request at a time
	retval += sem_init(&mem->requestNotif, 1, 0); // Notify of request
	retval += sem_init(&mem->responseNotif, 1, 0); // Notify of response of request
	retval += sem_init(&mem->spotEmptiedNotif, 1, 0); // Notify of empty spot
	retval += sem_init(&mem->enterMan, 1, 1); // Only one manoeuvre for entering at a time
	retval += sem_init(&mem->leaveMan, 1, 1); // Only one manoeuvre for leaving at a time
	retval += sem_init(&mem->ledgerAccess, 1, 1); // Access to ledger records
	retval += sem_init(&mem->statsAccess, 1, 1); // Access to stats
	retval += sem_init(&mem->countBusesWaitAccess, 1, 1); // Access to counter of buses waiting

	if (retval != 0) {
		perror("Mystation sem_init");

		return 5;
	}

	mem->countBusesWait = 0;
	mem->bayCapacity = bayCapacity;
	mem->maxNumPassengers = maxNumPassengers;
	mem->maxParkPeriod = maxParkPeriod;
	mem->initTime = initTime;

	mem->stats.totalGetOn = 0;
	mem->stats.totalGetOff = 0;
	for (int i = 0; i < 3; i++) {
		mem->numOfBuses[i] = 0;

		mem->stats.totalBuses[i] = 0;
		mem->stats.totalTimeStay[i] = 0;
		mem->stats.totalTimeWait[i] = 0;
	}


	//ledgerRecord *bays = (ledgerRecord *) mem + sizeof(sharedMem);
	ledgerRecord *bays = (ledgerRecord *) ((sharedMem *) mem + 1);
	for (int i = 0; i < 3 * (mem->bayCapacity); i++) {
		bays[i].arrivalTime = 0;
		strcpy(bays[i].busLicencePlate, "");
		strcpy(bays[i].busType, "");
		strcpy(bays[i].parkType, "");
		bays[i].numGetOff = 0;

		bays[i].statusFlag = 0;
	}

	fprintf(flog, "Mystation: Process created.\n");
	fprintf(flog, "Mystation: Initialized shared memory and ledger.\n");
	fprintf(flog, "Mystation: Creating all other processes needed.\n");
	fflush(flog);

	char maxInactiveBuff[SMALLBUFF];
	sprintf(maxInactiveBuff, "%d", maxInactive); // maxInactive to array of chars

	pid_t stationManagerId = fork();
	if (stationManagerId < 0) { // Check fork failure
		perror("Fork");

		return 5;
	}

	else if (stationManagerId == 0) { // Child process
		execlp("./station-manager", "station-manager", "-i", maxInactiveBuff, "-s", shmIdBuff, (char*) NULL);
		perror("Exec*"); // This command runs only if execlp fails

		return 6;
	}
	// Parent process

	char timeBuff[SMALLBUFF] = "4"; // Print status every 3 seconds
	char statTimesBuff[SMALLBUFF] = "8"; // Print stats every 3 seconds

	pid_t comptrollerId = fork();
	if (comptrollerId < 0) { // Check fork failure
		perror("Fork");

		return 5;
	}

	else if (comptrollerId == 0) { // Child process
		execlp("./comptroller", "comptroller", "-d", timeBuff, "-t", statTimesBuff, "-s", shmIdBuff, (char*) NULL);
		perror("Exec*"); // This command runs only if execlp fails

		return 6;
	}
	// Parent process

	for (int i = 0; i < numCreatedBuses; i++) {
		char licencePlate[8];
		sprintf(licencePlate, "Bus_%d", i + 1);

		char type[3];
		int typeRand = rand() % 3;
		if (typeRand == 0)
			strcpy(type, "ASK");
		else if (typeRand == 1)
			strcpy(type, "PEL");
		else // if (typeRand == 2)
			strcpy(type, "VOR");

		int incPassengers = 1 + rand() % maxNumPassengers;
		char incPassengersBuff[SMALLBUFF];
		sprintf(incPassengersBuff, "%d", incPassengers); // incPassengers to array of chars

		int capacity = 1 + rand() % maxNumPassengers;
		char capacityBuff[SMALLBUFF];
		sprintf(capacityBuff, "%d", capacity); // capacity to array of chars

		int parkPeriod = 1 + rand() % maxParkPeriod;
		char parkPeriodBuff[SMALLBUFF];
		sprintf(parkPeriodBuff, "%d", parkPeriod); // parkPeriod to array of chars

		char manTimeBuff[SMALLBUFF] = "3"; // Manoeuvre duration = 3 seconds

		pid_t busId = fork();
		if (busId < 0) { // Check fork failure
			perror("Fork");

			return 5;
		}

		else if (busId == 0) { // Child process
			execlp("./bus", "bus", "-l", licencePlate, "-t", type, "-n", incPassengersBuff, "-c", capacityBuff, "-p", parkPeriodBuff, "-m", manTimeBuff, "-s", shmIdBuff, (char*) NULL);
			perror("Exec*"); // This command runs only if execlp fails

			return 6;
		}
		// Parent process

		sleep(rand() % 2); // New bus every 2 seconds
	}

	pause();

	kill(comptrollerId, SIGUSR2); // Send SIGUSR2 Signal to comptrollerId

	retval = 0;
	retval += sem_destroy(&mem->requestAccess); // Only one request at a time
	retval += sem_destroy(&mem->requestNotif); // Notify of request
	retval += sem_destroy(&mem->responseNotif); // Notify of response of request
	retval += sem_destroy(&mem->spotEmptiedNotif); // Notify of empty spot
	retval += sem_destroy(&mem->enterMan); // Only one manoeuvre for entering at a time
	retval += sem_destroy(&mem->leaveMan); // Only one manoeuvre for leaving at a time
	retval += sem_destroy(&mem->ledgerAccess); // Access to ledger records
	retval += sem_destroy(&mem->statsAccess); // Access to stats
	retval += sem_destroy(&mem->countBusesWaitAccess); // Access to counter of buses waiting

	if (retval != 0) {
		perror("Mystation sem_destroy");

		return 7;
	}

	if (shmctl(shmId, IPC_RMID, 0) == -1) {
		perror("Mystation shmctl IPC_RMID");

		return 8;
	}

	fprintf(flog, "Mystation: Process finished.\n");
	fflush(flog);

	if (fclose(flog) == EOF) { // Check fclose failure
		printf("Error closing log file.\n");

		return 2;
	}
	flog = NULL;

	return 0;
}