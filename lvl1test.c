#include "types.h"
#include "user.h"

int
main()
{
	printf(1, "\nLevel 1 STARTING\n");

	int priority = 10;

	printf(1, "\nTest that priority cannot be raised higher than current process's priority\n");
	
	int status = prio_set(getpid(), priority); 
	printf(1, "\nSetting priority to: %d...%s\n", priority, status == 0 ? "PASSED" : "FAILED");

	int priority2 = 9; 
	int status2 = prio_set(getpid(), priority2); 
	printf(1, "\nSetting priorityy to: %d...%s\n", priority2, status2 == 0 ? "PASSED" : "FAILED");
	printf(1, "Priority set should fail since priority is being raised \n");

	wait(); 
	wait(); 
	
	exit();
	return 0;
}