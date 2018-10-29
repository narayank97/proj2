#include "VirtualMachine.h"
#include "Machine.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <vector>
#include <deque>
#include <new>
#include <iostream>
#include <dlfcn.h>
using namespace std;
//typedef unsigned int TVMStatus, *TVMStatusRef;
extern "C"{
	TVMMainEntry VMLoadModule(const char *module);
	TVMStatus VMFileOpen(const char *filename, int flags, int mode, int *filedescriptor);
	TVMStatus VMFileWrite(int filedescriptor, void *data, int *length);
	TVMStatus VMFileRead(int filedescriptor, void *data, int *length);
	void MachineFileOpen(const char *filename, int flags, int mode, TMachineFileCallback callback, void *calldata);
	typedef void (*TMachineFileCallback)(void *calldata, int result);
	void MachineContextCreate(SMachineContextRef mcntxref, void (*entry)(void *), void *param, void *stackaddr, size_t stacksize);
	void scheduler();
	void idle();

}


typedef struct{
	TVMMemorySize size;
	TVMStatus status;
	int result;
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
deque<TCB*> HIGH_priority;
deque<TCB*> MEDIUM_priority;
deque<TCB*> LOW_priority;

TVMThreadID current_ThreadID;
TCB *next_thread = new(TCB);
//int result;


int tick = 0;
typedef sigset_t TMachineSignalState, *TMachineSignalStateRef;
typedef void (*TVMMainEntry)(int, char*[]);
SMachineContextRef mcntxref;
void idle()
{
	while(1);
}


TVMStatus VMStart(int tickms, int argc, char *argv[])
{
	TVMMainEntry ret;
        const char *filename = strtok(argv[0], "\n");
        TVMThreadEntry entry = NULL;
        TVMThreadEntry entry_idle = NULL;
        void *handle = NULL;
        void *handle_idle = NULL;
	TVMMemorySize memsize = 100;
	printf("file: %s\n", filename);
        ret = VMLoadModule(filename);
        handle = dlopen(filename, RTLD_NOW);
        printf("after dlopen\n");
	entry = (TVMThreadEntry)dlsym(handle, "VMMain");
	//entry_idle = (TVMThreadEntry)dlsym(handle, "idle");
        printf("after entry\n");
	TVMThreadIDRef tid = (unsigned int*)0;
        VMThreadCreate(entry, argv, memsize, 0, tid);
        printf("after entry0\n");
	//handle_idle = dlopen("idle", RTLD_NOW);

	VMThreadCreate((void(*)(void*))"idle", argv, memsize, 1, (unsigned int*)1);
       	//MachineContextCreate(mcntxref, "idle", argv,


        printf("after entry1\n");
	if (ret == NULL) {
                printf("module didn't load\n");
                return VM_STATUS_FAILURE;
        }
        else {
                printf("about to run ret\n");
                ret(argc, argv);
                printf("after ret\n");
                return VM_STATUS_SUCCESS;
        }
        return 0;
}

void scheduler()
{
	SMachineContextRef previous_context;
	SMachineContextRef new_current_context;
	unsigned int i = 0;

		if(HIGH_priority[0] != NULL) //checks HIGH_priority queue
		{
			if(current_ThreadID == HIGH_priority[i] -> id)
			{
				previous_context = &HIGH_priority[i] -> context;
			}
			next_thread = HIGH_priority[i];
			HIGH_priority.pop_front();
			current_ThreadID = next_thread -> id;
			new_current_context = &next_thread -> context;
			next_thread -> state = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch(previous_context,new_current_context);
			return;
		}

		if(MEDIUM_priority[0] != NULL ) //checks MEDIUM_priority queue
		{
			if(current_ThreadID == MEDIUM_priority[i] -> id)
			{
				previous_context = &MEDIUM_priority[i] -> context;
			}
			next_thread = MEDIUM_priority[i];
			MEDIUM_priority.pop_front();
			current_ThreadID = next_thread -> id;
			new_current_context = &next_thread -> context;
			next_thread -> state = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch(previous_context,new_current_context);
			return;
		}

		if(LOW_priority[0] != NULL) //checks LOW_priority queue
		{
			if(current_ThreadID == LOW_priority[i] -> id)
			{
				previous_context = &LOW_priority[i] -> context;
			}
			next_thread = LOW_priority[i];
			LOW_priority.pop_front();
			current_ThreadID = next_thread -> id;
			new_current_context = &next_thread -> context;
			next_thread -> state = VM_THREAD_STATE_RUNNING;
			MachineContextSwitch(previous_context,new_current_context);
			return;
		}
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
	TMachineFileCallback callback = NULL;
	void *calldata = NULL;
	unsigned int thread_found = 0;
	TVMThreadID curr_id;
        unsigned int i;
 	printf("tcb size: %ld\n", TCB_ptrs.size());
	for(i = 0; i < TCB_ptrs.size(); i++)
        {
                curr_id = TCB_ptrs[i] -> id;
		printf("thread id: %p", curr_id);
                if(curr_id == current_ThreadID)
                {
                        thread_found = 1;
                        break;
                }
        }
	if (thread_found == 1)
	{
		calldata = &TCB_ptrs[i];
		TCB_ptrs[i] -> state = VM_THREAD_STATE_WAITING;
	}
	printf("threadfound: %d\n", thread_found);
	MachineFileOpen(filename, flags, mode, callback, &TCB_ptrs[i]);
	printf("before filereturn\n");
        //int filereturn = ((TCB*)TCB_ptrs[i])->result;
	//printf("result: %d\n", TCB_ptrs[i]->result);
	scheduler();

	//*filedescriptor = open(filename, flags, mode);
	/*if (*filedescriptor == -1){
		printf("file open failed\n");
		return VM_STATUS_FAILURE;
	}
	else if (*filedescriptor >= 0) return VM_STATUS_SUCCESS;*/


	return 0;
}

TVMStatus VMFileRead(int filedescriptor, void *data, int *length)
{
	if (length == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;
	TMachineFileCallback callback = NULL;
	void *calldata = NULL;
	unsigned int thread_found = 0;
	TVMThreadID curr_id;
        unsigned int i;
	for(i = 0; i < TCB_ptrs.size(); i++)
        {
                curr_id = TCB_ptrs[i] -> id;
                if(curr_id == current_ThreadID)
                {
                        thread_found = 1;
                        break;
                }
        }
	if (thread_found == 1)
	{
		TCB_ptrs[i] -> state = VM_THREAD_STATE_WAITING;
	}

	MachineFileRead(filedescriptor, data, *length, callback, calldata);
       	if (data == NULL) return VM_STATUS_ERROR_INVALID_PARAMETER;
	scheduler();
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
		printf("before entry\n");
		created_thread -> Entry = entry;
		printf("after entry\n");
		created_thread -> paramaters = param;
		created_thread -> result = 0;
		printf("before stackptr\n");
		created_thread -> stackPTR = (void*)malloc(100);
		printf("after stackptr\n");
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
TVMStatus VMFileSeek(int filedescriptor, int offset, int whence, int *newoffset)
{
	return 0;
}


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
