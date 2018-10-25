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


/*int main(int argc, char* argv[]) {
      VMStart(100, argc, argv);
 //   VMPrint("VMMain opening test.txt\n");
  //  VMLoadModule(argv[0]);
	return 0;
}*/

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
		TCB * created_thread = new(TCB);
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
	return 0;
}
TVMStatus VMThreadActivate(TVMThreadID thread)
{
	return 0;
}
TVMStatus VMThreadTerminate(TVMThreadID thread)
{
	return 0;
}
TVMStatus VMThreadID(TVMThreadIDRef threadref)
{
	if(threadref == NULL)
	{
		return VM_STATUS_ERROR_INVALID_PARAMETER;
	}

	else
	{
		/*TMachineSignalState signalState;
		MachineSuspendSignals(&signalState);
		threadref = current_thread -> id;
		MachineResumeSignals(&signalState);*/
		return VM_STATUS_SUCCESS;

	}

	return 0;
}
TVMStatus VMThreadState(TVMThreadID thread, TVMThreadStateRef stateref)
{
	//for(int i = 0; i < )
}
TVMStatus VMThreadSleep(TVMTick tick)
{
	return 0;
}


/*TVMStatus VMFileClose(int filedescriptor)
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
}*/
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
