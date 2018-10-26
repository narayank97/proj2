#include "VirtualMachine.h"
#include "Machine.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <vector>
#include <new>
#include <iostream>
#include <dlfcn.h>
using namespace std;
//typedef unsigned int TVMStatus, *TVMStatusRef;
void scheduler();
extern "C"{
	TVMMainEntry VMLoadModule(const char *module);
	void MachineFileOpen(const char *filename, int flags, int mode, TMachineFileCallback callback, void *calldata);

}


typedef struct{
	TVMMemorySize size;
	TVMStatus status;
	TVMTick ticks;
	TVMThreadID id;
	TVMThreadPriority priority;
	TVMThreadState state;
	SMachineContext context;
	void *stackPTR;
	TVMThreadEntry Entry;
	void* paramaters;
}TCB;

vector<TCB*> TCB_ptrs; // vector for threads
vector<TCB*> ready_threads;
vector<TCB*> HIGH_priority;
vector<TCB*> MEDIUM_priority;
vector<TCB*> LOW_priority;

TVMThreadID current_ThreadID;
TCB *next_thread = new(TCB);


void scheduler()
{
	SMachineContextRef previous_context;
	SMachineContextRef new_current_context;
	unsigned int i = 0;
	for(i = 0; i < HIGH_priority.size(); i++) // loops through HIGH_priority threads
	{
		if(HIGH_priority[i] == NULL)
		{
			break;
		}
		else
		{
			if(current_ThreadID == HIGH_priority[i] -> id)
			{
				previous_context = &HIGH_priority[i] -> context;
			}
			next_thread = HIGH_priority[i];
			current_ThreadID = next_thread -> id;
			new_current_context = &next_thread -> context;
			next_thread -> state = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch(previous_context,new_current_context);
			return;
		}
	}
	for(i = 0; i < MEDIUM_priority.size(); i++) // loops through Medium Priority threads
	{
		if(MEDIUM_priority[i] == NULL)
		{
			break;
		}
		else
		{
			if(current_ThreadID == MEDIUM_priority[i] -> id)
			{
				previous_context = &MEDIUM_priority[i] -> context;
			}
			next_thread = MEDIUM_priority[i];
			current_ThreadID = next_thread -> id;
			new_current_context = &next_thread -> context;
			next_thread -> state = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch(previous_context,new_current_context);
			return;
		}
	}
	for(i = 0; i < LOW_priority.size(); i++) // Loops through low priority thrads
	{
		if(LOW_priority[i] == NULL)
		{
			break;
		}
		else // if it exists then next thread becomes the earliest TCB
		{
			if(current_ThreadID == LOW_priority[i] -> id)
			{
				previous_context = &LOW_priority[i] -> context;
			}
			next_thread = LOW_priority[i];
			current_ThreadID = next_thread -> id;
			new_current_context = &next_thread -> context;
			next_thread -> state = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch(previous_context,new_current_context);
			return;
		}
	}


}

int tick = 0;
typedef sigset_t TMachineSignalState, *TMachineSignalStateRef;
typedef void (*TVMMainEntry)(int, char*[]);

TVMStatus VMStart(int tickms, int argc, char *argv[])
{
	TVMMainEntry ret;
        const char *filename = strtok(argv[0], "\n");
        ret = VMLoadModule(filename);
        if (ret == NULL) {
                printf("module didn't load\n");
                return VM_STATUS_FAILURE;
        }
        else {
                ret(argc, argv);
                return VM_STATUS_SUCCESS;
        }
        return 0;
}

TVMStatus VMFileWrite(int filedescriptor, void *data, int *length)
{
	if (data == NULL || length == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;
	TMachineSignalState sigstate;
	//TMachineSignalStateRef sigstateref;
	MachineSuspendSignals(&sigstate);
	int bytes_written = write(filedescriptor, data, (size_t)*length);
	if (bytes_written == -1) return VM_STATUS_FAILURE;
	else if (bytes_written >= 0) return VM_STATUS_SUCCESS;
	MachineResumeSignals(&sigstate);
	return 0;
}

TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor)
{
	printf("entered fileopen\n");
	if (filename == NULL || filedescriptor == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;
	TMachineFileCallback callback;
	void *calldata;
	MachineFileOpen(filename, flags, mode, callback, calldata);
	//*filedescriptor = open(filename, flags, mode);
	/*if (*filedescriptor == -1){
		printf("file open failed\n");
		return VM_STATUS_FAILURE;
	}
	else if (*filedescriptor >= 0) return VM_STATUS_SUCCESS;*/


	return 0;
}

TVMStatus VMTickMS(int *tickmsref)
{
	*tickmsref = 100;
	return 0;
}
TVMStatus VMTickCount(TVMTickRef tickref)
{
	if(tickref == NULL)
	{
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else
	{
		TMachineSignalState signalState;
  		MachineSuspendSignals(&signalState);
		*tickref = tick;
		MachineResumeSignals(&signalState);
  		return VM_STATUS_SUCCESS;
	}

}

TVMStatus VMThreadCreate(TVMThreadEntry entry, void *param, TVMMemorySize memsize, TVMThreadPriority prio, TVMThreadIDRef tid)
{
	if(tid == NULL || entry == NULL)
	{
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	else
	{
		TMachineSignalState signalState;
  		MachineSuspendSignals(&signalState);
		TCB *created_thread = new(TCB);
		created_thread -> size = memsize;
		created_thread -> id = *tid;
		created_thread -> priority = prio;
		created_thread -> state = VM_THREAD_STATE_DEAD;
		created_thread -> Entry = entry;
		created_thread -> paramaters = param;
		TCB_ptrs.push_back(created_thread);
		MachineResumeSignals(&signalState);
		return VM_STATUS_SUCCESS;

	}
}
TVMStatus VMThreadDelete(TVMThreadID thread)
{
	TMachineSignalState signalState;
	MachineSuspendSignals(&signalState);

	int thread_found = 0; // its false
	unsigned int i = 0;

	TVMThreadID curr_id;
	for(i = 0; i < TCB_ptrs.size(); i++)
	{
		curr_id = TCB_ptrs[i] -> id;
		if(curr_id == thread)
		{
			thread_found = 1;
			break;
		}
	}
	if(thread_found == 1)
	{
		if(TCB_ptrs[i]-> state == VM_THREAD_STATE_DEAD)
		{
	 		TCB_ptrs[i] = NULL;
			MachineResumeSignals(&signalState);
			return VM_STATUS_SUCCESS;
		}
		else
		{
			MachineResumeSignals(&signalState);
			return VM_STATUS_ERROR_INVALID_STATE;
		}
	}
	else
	{
		MachineResumeSignals(&signalState);
		return VM_STATUS_ERROR_INVALID_ID;
	}
}
TVMStatus VMThreadActivate(TVMThreadID thread)
{
	TMachineSignalState signalState;
	MachineSuspendSignals(&signalState);

	int thread_found = 0; // its false
	unsigned int i = 0;

	TVMThreadID curr_id;
	for(i = 0; i < TCB_ptrs.size(); i++)
	{
		curr_id = TCB_ptrs[i] -> id;
		if(curr_id == thread)
		{
			thread_found = 1;
			break;
		}
	}
	if(thread_found == 1)
	{
		if(TCB_ptrs[i]-> state == VM_THREAD_STATE_DEAD)
		{
	 		TCB_ptrs[i] -> state = VM_THREAD_STATE_READY;
			MachineResumeSignals(&signalState);
			return VM_STATUS_SUCCESS;
		}
		else
		{
			MachineResumeSignals(&signalState);
			return VM_STATUS_ERROR_INVALID_STATE;
		}
	}
	else
	{
		MachineResumeSignals(&signalState);
		return VM_STATUS_ERROR_INVALID_ID;
	}

	return 0;
}
TVMStatus VMThreadTerminate(TVMThreadID thread)
{
	TMachineSignalState signalState;
	MachineSuspendSignals(&signalState);

	int thread_found = 0; // its false
	unsigned int i = 0;

	TVMThreadID curr_id;
	for(i = 0; i < TCB_ptrs.size(); i++)
	{
		curr_id = TCB_ptrs[i] -> id;
		if(curr_id == thread)
		{
			thread_found = 1;
			break;
		}
	}
	if(thread_found == 1)
	{
		if(TCB_ptrs[i]-> state == VM_THREAD_STATE_DEAD)
		{
			MachineResumeSignals(&signalState);
			return VM_STATUS_ERROR_INVALID_STATE;
		}
		else
		{
			TCB_ptrs[i] -> state = VM_THREAD_STATE_DEAD;
			// schedule another thread HERE
			MachineResumeSignals(&signalState);
			return VM_STATUS_SUCCESS;
		}
	}
	else
	{
		MachineResumeSignals(&signalState);
		return VM_STATUS_ERROR_INVALID_ID;
	}

}
TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref == NULL)
	{
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	else
	{
		TMachineSignalState signalState;
		MachineSuspendSignals(&signalState);
		threadref = &current_ThreadID;
		MachineResumeSignals(&signalState);
		return VM_STATUS_SUCCESS;
	}

	return 0;
}
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	TMachineSignalState signalState;
	MachineSuspendSignals(&signalState);

	int thread_found = 0; // its false
	unsigned int i = 0;

	if(stateref == NULL)
	{
		MachineResumeSignals(&signalState);
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}
	TVMThreadID curr_id;
	for(i = 0; i < TCB_ptrs.size(); i++)
	{
		curr_id = TCB_ptrs[i] -> id;
		if(curr_id == thread)
		{
			thread_found = 1;
			break;
		}
	}
	if(thread_found == 1)
	{
		*stateref = TCB_ptrs[i] -> state;
		MachineResumeSignals(&signalState);
		return VM_STATUS_SUCCESS;
	}
	else
	{
		MachineResumeSignals(&signalState);
		return VM_STATUS_ERROR_INVALID_ID;
	}
}
TVMStatus VMThreadSleep(TVMTick tick)
{
	return 0;
}


TVMStatus VMFileClose(int filedescriptor)
{
	return 0;
}
TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{
	return 0;
}
TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{
	return 0;
}

/*TVMStatus VMFilePrint(int filedescriptor, const char *format, ...)
{
	return 0;
}*/

/*TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref == NULL)
	{
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	else
	{
		TMachineSignalState signalState;
		MachineSuspendSignals(&signalState);
		*threadref = current_thread -> id;
		MachineResumeSignals(&signalState);
		return VM_STATUS_SUCCESS;

	}

	return 0;
}*/
