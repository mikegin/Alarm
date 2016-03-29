/*
 * My_Alarm.c
 *
 * This is an enhancement to the alarm_mutex.c program. 
 * Instead of alarms expiring in a linked list, each alarm 
 * is designated it's own individual thread which expires
 * when the alarms' time runs out.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure contains a link to the next element 
 * in the linked list, the time_t (time since the Epoch, in 
 * seconds), the number of seconds for the alarm, as well as
 * the message to be printed.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    time_t              time;   /* seconds from EPOCH */
    char                message[64];
} alarm_t;

/*
* Mutex for the alarm list.
*/
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;

/*
* Flag for when there is no more input.
*/
int alarm_done = 0;

/*
 *new thread to be created by alarm thread. handles individual alarms
 *thread will be disintegrated when alarm time expires.
 */
void *individual_alarm_thread (void *arg)
{
	alarm_t *alarm = (alarm_t *)arg;
	int sleep_time;
	time_t now;
	while(1)
	{
		now = time (NULL);
		//Alarm has expired
		if(alarm->time <= now)
		{
			printf ("Alarm Expired at %d:%d %s\n", now, alarm->time, alarm->message);
			free (alarm);
			pthread_exit(0);
		}
		//Print message and time left
		else
		{
			/*tick*/
            printf ("Alarm:%d %s\n", alarm->time, alarm->message);
			sleep (1);
		}
	}
	
}
/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;
    pthread_t thread;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the main indicates there is no more
     * input and the alarm list is empty.
     */
    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        
        /*
        * Lock the mutex, preventing main from receiving new alarms.
        */
        if (status != 0)
            err_abort (status, "Lock mutex");
               
        alarm = alarm_list;
               
         /*
         * If there is more input on the way but the list is empty.
         */
        if (alarm == NULL && alarm_done == 0)
            sleep_time = 1;
         
        /*
        * Main thread has terminated and the alarm list is empty. Terminate alarm_thread.
        */
        else if(alarm == NULL && alarm_done == 1)
        	pthread_exit(0);
        
        /*
        * Alarm retrieved. Create the individual thread.
        */
        else
        {
        	
            printf("Alarm Retrieved at %d:%d %s\n",time (NULL), alarm->time, alarm->message);
            alarm_list=alarm->link;
        	status = pthread_create(&thread,NULL,individual_alarm_thread,alarm);
        	if(status != 0)
        		err_abort (status,"create individual alarm thread");
        }

        /*
        * Unlock the mutex, allowing the main thread to receive new alarms.
        */
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
    }
}

int main (int argc, char *argv[])
{
    int status;
    char line[128];
    alarm_t *alarm, **last, *next;
    pthread_t thread;

    /*
    * Create the alarm_thread thread
    */
    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    /*
    * Loop indefinitely.
    */
    while (1) {
        printf ("alarm> ");
        /*
        * If there is no more input, set the flag and terminitate the main thread.
        */
        if (fgets (line, sizeof (line), stdin) == NULL)
        {
        	alarm_done=1;
        	pthread_exit(0);
        }
        
        if (strlen (line) <= 1) continue;
        
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into seconds (%d) and a message
         * (%64[^\n]), consisting of up to 64 characters
         * separated from the seconds by whitespace.
         */
        if (sscanf (line, "%d %64[^\n]", 
            &alarm->seconds, alarm->message) < 2) {
            fprintf (stderr, "Bad command\n");
            free (alarm);
        } else {
        	/*
        	* Lock the mutex, preventing alarm_thread from creating individual threads.
        	*/
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            alarm->time = time (NULL) + alarm->seconds;
            
            /*alarm recieved*/
            printf("Alarm Recieved at %d:%d %s\n",time (NULL), alarm->time, alarm->message);
            
            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */
            last = &alarm_list;
            next = *last;
            while (next != NULL) {
                if (next->time >= alarm->time) {
                    alarm->link = next;
                    *last = alarm;
                    break;
                }
                last = &next->link;
                next = next->link;
            }
            /*
             * If we reached the end of the list, insert the new
             * alarm there. ("next" is NULL, and "last" points
             * to the link field of the last item, or to the
             * list header).
             */
            if (next == NULL) {
                *last = alarm;
                alarm->link = NULL;
            }
            
            /*
            * Unlock the alarm_list, allowing alarm_thread to create the individual thread.
            */
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
        }
    }
}
