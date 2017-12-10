// scheduler.cc 
//	Routines to choose the next thread to run, and to dispatch to
//	that thread.
//
// 	These routines assume that interrupts are already disabled.
//	If interrupts are disabled, we can assume mutual exclusion
//	(since we are on a uniprocessor).
//
// 	NOTE: We can't use Locks to provide mutual exclusion here, since
// 	if we needed to wait for a lock, and the lock was busy, we would 
//	end up calling FindNextToRun(), and that would put us in an 
//	infinite loop.
//
// 	Very simple implementation -- no priorities, straight FIFO.
//	Might need to be improved in later assignments.
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "debug.h"
#include "scheduler.h"
#include "main.h"

//----------------------------------------------------------------------
// Scheduler::Scheduler
// 	Initialize the list of ready but not running threads.
//	Initially, no ready threads.
//----------------------------------------------------------------------

Scheduler::Scheduler()
{ 
    //readyList = new List<Thread *>; 
    readyListSJF = new SortedList<Thread *>(Thread::compBurst);
    readyListPri = new SortedList<Thread *>(Thread::compPriority);
    readyListRR = new List<Thread *>;

    toBeDestroyed = NULL;
} 

//----------------------------------------------------------------------
// Scheduler::~Scheduler
// 	De-allocate the list of ready threads.
//----------------------------------------------------------------------

Scheduler::~Scheduler()
{ 
    //delete readyList; 
    delete readyListSJF;
    delete readyListPri;
    delete readyListRR;
} 

//----------------------------------------------------------------------
// Scheduler::ReadyToRun
// 	Mark a thread as ready, but not running.
//	Put it on the ready list, for later scheduling onto the CPU.
//
//	"thread" is the thread to be put on the ready list.
//----------------------------------------------------------------------

void
Scheduler::ReadyToRun (Thread *thread)
{
    //cout << "[DEBUG]\tEnter Scheduler::ReadyToRun()" << endl;
    ASSERT(kernel->interrupt->getLevel() == IntOff);
    DEBUG(dbgThread, "Putting thread on ready list: " << thread->getName());
	//cout << "Putting thread on ready list: " << thread->getName() << endl ;
    thread->setStatus(READY);
    //readyList->Append(thread);
    if(strcmp(thread->getName(), "postal worker")==0) return;
    int cur_priority = thread->getPriority();

    if (cur_priority < 50) {
        readyListRR->Append(thread);
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L3" << endl;
    }
    else if (cur_priority >= 50 && cur_priority < 100) {
        readyListPri->Insert(thread);
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L2" << endl;
    }
    else if (cur_priority >= 100 && cur_priority < 150) {
        readyListSJF->Insert(thread);
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << thread->getID() << " is inserted into queue L1" << endl;
    }
}

//----------------------------------------------------------------------
// Scheduler::FindNextToRun
// 	Return the next thread to be scheduled onto the CPU.
//	If there are no ready threads, return NULL.
// Side effect:
//	Thread is removed from the ready list.
//----------------------------------------------------------------------

Thread *
Scheduler::FindNextToRun ()
{
    //cout << "[DEBUG]\tEnter Scheduler::FindNextToRun()" << endl;
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    Aging(readyListSJF, readyListPri, readyListRR);

    /*if (readyListSJF->IsEmpty()) {
        if (readyListPri->IsEmpty()) {
            if (readyListRR->IsEmpty()) {
                return NULL;
            }
            else {
                Thread *front = readyListRR->RemoveFront();
                cout << "Tick " << kernel->stats->totalTicks << ": Thread " << front->getID() << " is removed from queue L3" << endl;
                return front;
            }
        }
        else {
            Thread *front = readyListPri->RemoveFront();
            cout << "Tick " << kernel->stats->totalTicks << ": Thread " << front->getID() << " is removed from queue L2" << endl;
            return front;
        }
    }
    else {
        Thread *front = readyListSJF->RemoveFront();
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << front->getID() << " is removed from queue L1" << endl;
        return front;
    }*/

    if (!readyListSJF->IsEmpty()) {
        Thread *front = readyListSJF->RemoveFront();
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << front->getID() << " is removed from queue L1" << endl;
        return front;
    }
    else if (!readyListPri->IsEmpty()) {
        Thread *front = readyListPri->RemoveFront();
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << front->getID() << " is removed from queue L2" << endl;
        return front;
    }
    else if (!readyListRR->IsEmpty()) {
        Thread *front = readyListRR->RemoveFront();
        cout << "Tick " << kernel->stats->totalTicks << ": Thread " << front->getID() << " is removed from queue L3" << endl;
        return front;
    }
    else{
        return NULL;
    }
/*
    if (readyList->IsEmpty()) {
		return NULL;
    } else {
    	return readyList->RemoveFront();
    }*/
}

//----------------------------------------------------------------------
// Scheduler::Run
// 	Dispatch the CPU to nextThread.  Save the state of the old thread,
//	and load the state of the new thread, by calling the machine
//	dependent context switch routine, SWITCH.
//
//      Note: we assume the state of the previously running thread has
//	already been changed from running to blocked or ready (depending).
// Side effect:
//	The global variable kernel->currentThread becomes nextThread.
//
//	"nextThread" is the thread to be put into the CPU.
//	"finishing" is set if the current thread is to be deleted
//		once we're no longer running on its stack
//		(when the next thread starts running)
//----------------------------------------------------------------------

void
Scheduler::Run (Thread *nextThread, bool finishing)
{
    //cout << "[DEBUG]\tEnter Scheduler::Run()" << endl;
    Thread *oldThread = kernel->currentThread;
    
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    if (finishing) {	// mark that we need to delete current thread
         ASSERT(toBeDestroyed == NULL);
	 toBeDestroyed = oldThread;
    }
    
    if (oldThread->space != NULL) {	// if this thread is a user program,
        oldThread->SaveUserState(); 	// save the user's CPU registers
	oldThread->space->SaveState();
    }
    
    oldThread->CheckOverflow();		    // check if the old thread
					    // had an undetected stack overflow

    kernel->currentThread = nextThread;  // switch to the next thread
    nextThread->setStatus(RUNNING);      // nextThread is now running
    
    DEBUG(dbgThread, "Switching from: " << oldThread->getName() << " to: " << nextThread->getName());
    
    cout << "Tick " << kernel->stats->totalTicks << ": Thread " << nextThread->getID() << " is now selected for execution" << endl;
    cout << "Tick " << kernel->stats->totalTicks << ": Thread " << oldThread->getID() << " is replaced, and it has executed" << endl;

    if (oldThread->getStartBurst() != 0) {
        oldThread->setBurstTime(kernel->stats->totalTicks);
    }
    else {
        oldThread->setStartBurst();
    }

    kernel->currentThread->setStartBurst();
    // This is a machine-dependent assembly language routine defined 
    // in switch.s.  You may have to think
    // a bit to figure out what happens after this, both from the point
    // of view of the thread and from the perspective of the "outside world".

    SWITCH(oldThread, nextThread);

    // we're back, running oldThread
      
    // interrupts are off when we return from switch!
    ASSERT(kernel->interrupt->getLevel() == IntOff);

    DEBUG(dbgThread, "Now in thread: " << oldThread->getName());

    CheckToBeDestroyed();		// check if thread we were running
					// before this one has finished
					// and needs to be cleaned up
    
    if (oldThread->space != NULL) {	    // if there is an address space
        oldThread->RestoreUserState();     // to restore, do it.
	oldThread->space->RestoreState();
    }
}

//----------------------------------------------------------------------
// Scheduler::CheckToBeDestroyed
// 	If the old thread gave up the processor because it was finishing,
// 	we need to delete its carcass.  Note we cannot delete the thread
// 	before now (for example, in Thread::Finish()), because up to this
// 	point, we were still running on the old thread's stack!
//----------------------------------------------------------------------

void
Scheduler::CheckToBeDestroyed()
{
    if (toBeDestroyed != NULL) {
        delete toBeDestroyed;
	toBeDestroyed = NULL;
    }
}
 
//----------------------------------------------------------------------
// Scheduler::Print
// 	Print the scheduler state -- in other words, the contents of
//	the ready list.  For debugging.
//----------------------------------------------------------------------
void
Scheduler::Print()
{
    cout << "Ready list contents:\n";
    readyListRR->Apply(ThreadPrint);
}


void
Scheduler::Aging(SortedList<Thread *>* li1, SortedList<Thread *>* li2, List<Thread *>* li3)
{
    ListIterator<Thread*> * It_li1 = new ListIterator<Thread *>((List<Thread *> *)li1);
    ListIterator<Thread*> * It_li2 = new ListIterator<Thread *>((List<Thread *> *)li2);
    ListIterator<Thread*> * It_li3 = new ListIterator<Thread *>((List<Thread *> *)li3);
    
    for(; !It_li1->IsDone(); It_li1->Next()){
        Thread *cur = It_li1->Item();
        if((kernel->stats->totalTicks) - (cur->getReadyTime()) >= 1500){
            cur->setReadyTime();
            if(cur->getPriority()+10 < 150){
                cur->setPriority(cur->getPriority()+10);
            }
            else{
                cur->setPriority(149);
            }
        }
    }

    for(; !It_li2->IsDone(); It_li2->Next()){
        Thread *cur = It_li2->Item();
        if((kernel->stats->totalTicks) - (cur->getReadyTime()) >= 1500){
            cur->setReadyTime();
            if(cur->getPriority()+10 >= 100){
                cur->setPriority(cur->getPriority()+10);
                kernel->scheduler->readyListPri->Remove(cur);
                cout << "Tick" << kernel->stats->totalTicks << ": Thread " << cur->getID() << " is removed from queue L2" << endl;
                kernel->scheduler->readyListSJF->Insert(cur);
                cout << "Tick" << kernel->stats->totalTicks << ": Thread " << cur->getID() << " is inserted into queue L1" << endl;
            }
            else{
                cur->setPriority(cur->getPriority()+10);
            }
        }
    }

    for(; !It_li3->IsDone(); It_li3->Next()){
        Thread *cur = It_li3->Item();
        if((kernel->stats->totalTicks) - (cur->getReadyTime()) >= 1500){
            cur->setReadyTime();
            if(cur->getPriority()+10 >= 50){
                cur->setPriority(cur->getPriority()+10);
                kernel->scheduler->readyListRR->Remove(cur);
                cout << "Tick" << kernel->stats->totalTicks << ": Thread " << cur->getID() << " is removed from queue L3" << endl;
                kernel->scheduler->readyListPri->Insert(cur);
                cout << "Tick" << kernel->stats->totalTicks << ": Thread " << cur->getID() << " is inserted into queue L2" << endl;
            }
            else{
                cur->setPriority(cur->getPriority()+10);
            }
        }
    }
}
