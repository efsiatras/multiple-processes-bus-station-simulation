#ifndef DATA_STRUCTS_H
#define DATA_STRUCTS_H

typedef struct { // A record in ledger
	time_t arrivalTime;

	char busLicencePlate[9];
	char busType[4];
	char parkType[4];

	int numGetOff;
	int statusFlag; // 0: Left, 1: Parked, 2: About to park, 3: About to leave 
} ledgerRecord;

typedef struct { // Total statistics
	int totalGetOn; // Total number of people that got on buses
	int totalGetOff;  // Total number of people that got off buses

	int totalBuses[3]; // Total buses that stayed and left
	time_t totalTimeStay[3];
	time_t totalTimeWait[3];
} statsInfo;

typedef struct { // Request to station-manager
	time_t busArrivalTime;
	char busLicencePlate[8];
	char busType[3];
	int busNumGetOff;

	int typeFlag; // 1: Request for entering
				  // 2: Request for leaving
				  // 3: Request to inform manoeuvre is over (update from 'About to park' to 'Parked')
				  // 4: Request to inform manoeuvre is over (update from 'About to leave' to 'Left')

	int responseFlag; // 0: Rejected request
					  // 1: Accepted request
} requestInfo;

typedef struct {
	sem_t requestAccess; // Only one request at a time
	sem_t requestNotif; // Notify of request
	sem_t responseNotif; // Notify of response of request

	sem_t spotEmptiedNotif; // Notify of empty spot

	sem_t enterMan; // Only one manoeuvre for entering at a time
	sem_t leaveMan; // Only one manoeuvre for leaving at a time

	sem_t ledgerAccess; // Access to ledger records
	sem_t statsAccess; // Access to stats

	sem_t countBusesWaitAccess; // Access to counter of buses waiting
	int countBusesWait; // Number of buses waiting for a spot to empty

	int numOfBuses[3]; // Number of buses in each bay ; Considered part of ledger

	statsInfo stats;
	requestInfo request; 

	int bayCapacity; // Capacity of each bay ; Read-only access after initialization
	int maxNumPassengers; // Maximum number of passengers for each bus ; Read-only access after initialization
	int maxParkPeriod; // Maximum number of park period ; Read-only access after initialization
	time_t initTime; // Read-only access after initialization
} sharedMem;

#endif