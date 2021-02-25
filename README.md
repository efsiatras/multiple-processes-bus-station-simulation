# Bus Station Simulation with Multiple Processes
A program simulating a bus station with multiple and different types of processes using semaphores and shared memory.

The main goal of this program implementation was to practice with **fork**, **exec**, **semaphores** and **shared memory segment**.

## Diagram
![Diagram of bus station](https://siatras.dev/img/bus-station.jpg)

## Compilation
`$ make clean`

`$ make`

## Usage
Command Line Flag **l** is required.

`./mystation -l configfile`

## Configuration File
In order to execute Mystation, a configuration file is needed in the following form:

- Number of bays (must be 3 with current implementation, as noted in piazza)
- Capacity of each bay
- Maximum number of passengers in each bus
- Maximum period of parking for each bus
- Number of buses created by Mystation
- Maximum time of inactivity for Station-Manager to finish execution

## Executables
- **bus**
- **station-manager**
- **comptroller**
- **mystation**

## Implementation
- **Header file with structs used**: data-structs.h
- **Main function for Bus**: bus.c
- **Main function for Station-Manager**: station-manager.c
- **Main function for Comptroller**: comptroller.c
- **Main function for Mystation**: mystation.c

## Communication Protocol / Execution Path
**Mystation** starts.  
**Mystation** initializes shared memory and fork/exec* all other necessary processes (Station-Manager, Comptroller and many Buses).  
Station-Manager, Comptroller are executed once by **Mystation**.  
One bus every short period of time is executed by **Mystation**.  

**Buses** fill in requests (only one at a time with semaphore 'requestAccess') and waits for a response.  
**Station-Manager** waits for a request with semaphore 'requestNotif', creating a queue of buses wanting to fill in requests.  
**Station-Manager** responds to the request with semaphore 'responseNotif'.  

There are **4 types of requests by Buses**:
1. Request for **entering**
2. Request for **leaving**
3. Request to inform **manoeuvre is over** (update from **'About to park' to 'Parked'**)
4. Request to inform **manoeuvre is over** (update from **'About to leave' to 'Left'**)

There are **2 types of responses by Station-Manager**:

0. **Rejected** request
1. **Accepted** request

If **Station-Manager** cannot find a park spot for a **Bus**, then **Station-Manager** rejects its request (**request 1**).  
Then, the **Bus** is placed in a queue, waiting for a parking spot to be emptied with semaphore `spotEmptiedNotif`.  
When a parking spot is emptied, then these **waiting Buses** get notified to request for entering again.  

If **Station-Manager** finds a park spot for a bus, then **Station-Manager** assigns that spot to the bus (**request 1**).  
The **Bus** waits for previous incoming buses manoeuvring with semaphore `enterMan`.  
When there is no other **incoming Bus** manoeuvring, the **Bus** goes in and manoeuvres.  
When the **Bus** finishes manoeuvring, it informs the **Station-Manager** that it has parked (**request 3**) and it sleeps the maximum park period.  
When the **Bus** wakes up, it informs the **Station-Manager** that it wants to leave (**request 2**).  
The **Bus** waits for previous outgoing buses manoeuvring with semaphore `leaveMan`.  
When there is no other **outgoing Bus** manoeuvring, the bus goes out and manoeuvres.  
When the **Bus** finishes manoeuvring, it informs the **Station-Manager** that it has left (**request 4**).  
Then, the **Bus** finishes execution and **Station-Manager** frees park spot for other buses.  

The **ledger in shared memory** can be accessed by only one process with semaphore `ledgerAccess`.

The **stats in shared memory** can be accessed by only one process with semaphore `statsAccess`.

**Comptroller** prints the current snapshot of the ledger every specific period of time.  
**Comptroller** also prints some stats every another specific period of time.

When **Mystation** stops creating new **Buses** and all **Buses** leave, then **Station-Manager** will eventually get blocked by semaphore `requestNotif`.  
The semaphore `requestNotif` is called with `sem_timedwait` and as parameter the maximum time of inactivity for **Station-Manager** from configuration file.  
When that maximum time of inactivity exceeds, then **Station-Manager** sends a `SIGUSR2` signal to Mystation.  
That signal informs **Mystation** that all Buses have finished execution and **Station-Manager** is heading towards the end of execution.  
Then, **Mystation** sends the same `SIGUSR2` signal to **Comptroller** to stop printing and finish execution.

All executables finish execution.  

Important events are being logged to a log file.
