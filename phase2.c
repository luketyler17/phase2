/* ------------------------------------------------------------------------
   phase2.c
   Applied Technology
   College of Applied Science and Technology
   The University of Arizona
   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include <string.h>
#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1(char *);
extern int start2(char *);
void disableInterrupts();
void enableInterrupts();
void shift_pids(mail_box *input);
void add_pid(mail_box *input);

/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* the mail boxes */
mail_box MailBoxTable[MAXMBOX];
slot_ptr *CurrentMbox;
/* -------------------------- Functions -----------------------------------
  Below I have code provided to you that calls

  check_kernel_mode
  enableInterrupts
  disableInterupts

  These functions need to be redefined in this phase 2,because
  their phase 1 definitions are static
  and are not supposed to be used outside of phase 1.  */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg)
{
   int kid_pid, status;

   if (DEBUG2 && debugflag2)
      console("start1(): at beginning\n");

   // check_kernel_mode("start1");

   /* Disable interrupts */
   disableInterrupts();

   /* Initialize the mail box table, slots, & other data structures.
    * Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */
   for (int i = 0; i < MAXMBOX; i++)
   {
      // Initializing the mailbox table
      MailBoxTable[i].mbox_id = -1;

      for (int j = 0; j < MAXPROC; j++)
      {
         MailBoxTable[i].pid[j] = -1;
      }
   }

   enableInterrupts();

   /* Create a process for start2, then block on a join until start2 quits */
   if (DEBUG2 && debugflag2)
      console("start1(): fork'ing start2 process\n");
   kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
   if (join(&status) != kid_pid)
   {
      console("start2(): join returned something other than start2's pid\n");
   }

   return 0;
} /* start1 */

/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array.
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size)
{
   int mailbox_id = -1;

   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not create mailbox\n");
      halt(1);
   }

   if (slots > MAXSLOTS)
   {
      console("Error: requested slots greater than maxium allowable slot number\n");
      halt(1);
   }

   if (slot_size > MAX_MESSAGE)
   {
      console("Error: requested slot size greater than maxiumum allowable message size\n");
   }

   // find an empty ID slot on the table
   int i = 0;
   while (1)
   {
      if (MailBoxTable[i].mbox_id == -1)
      {
         MailBoxTable[i].mbox_id = i;
         MailBoxTable[i].num_slots = slots;
         mailbox_id = i;
         break;
      }
      if (i >= MAXMBOX)
      {
         console("Error: No mailboxes available");
         return -1;
      }
      i++;
   }

   // create mailbox slots inside of mailbox
   for (int j = 0; j < slots; j++)
   {
      MailBoxTable[mailbox_id].mail_slot_array[j] = malloc(sizeof(slot_ptr));
      MailBoxTable[mailbox_id].mail_slot_array[j]->mbox_id = mailbox_id;
      MailBoxTable[mailbox_id].mail_slot_array[j]->status = EMPTY;
   }

   // return the Id of the mailbox
   return mailbox_id;
} /* MboxCreate */

/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
   slot_ptr *CurrentMbox = MailBoxTable[mbox_id].mail_slot_array;
   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not send message\n");
      halt(1);
   }
   if (is_zapped())
   {
      return -3;
   }
   // check if msg_size is greater than max allowed
   // check if mbox_id is initialized on MailBoxTable
   while (1)
   {
      for (int i = 0; i < MailBoxTable[mbox_id].num_slots; i++)
      {
         if (CurrentMbox[i] != NULL)
         {
            if (CurrentMbox[i]->status == EMPTY)
            {
               memcpy(&(CurrentMbox[i]->message), msg_ptr, msg_size);
               CurrentMbox[i]->status = FULL;
               if (MailBoxTable[mbox_id].is_blocked == 1 && MailBoxTable[mbox_id].block_reason == BLOCK_SELF)
               {
                  int pid = MailBoxTable[mbox_id].pid[0];
                  shift_pids(&MailBoxTable[mbox_id]);
                  if (MailBoxTable[mbox_id].pid[0] != -1)
                  {
                     unblock_proc(pid);
                     return 0;
                  }
                  else
                  {
                     MailBoxTable[mbox_id].block_reason = 0;
                     MailBoxTable[mbox_id].is_blocked = 0;
                     unblock_proc(pid);
                  }
               }
               return 0;
            }
         }
      }
      // couldn't find an empty mailbox, blocking process until it empties a mailslot
      MailBoxTable[mbox_id].block_reason = BLOCK_SELF;
      add_pid(&MailBoxTable[mbox_id]);
      MailBoxTable[mbox_id].is_blocked = 1;
      block_me(11);
   }

} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args, -3 if process is zapped
             mailbox was released while process was blocked on mailbox
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
   slot_ptr *CurrentMbox = MailBoxTable[mbox_id].mail_slot_array;

   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not send message\n");
      halt(1);
   }
   if (is_zapped())
   {
      return -3;
   }
   // check if msg_size is greater than max allowed
   // check if mbox_id is initialized on MailBoxTable

   while (1)
   {
      for (int i = 0; i < MailBoxTable[mbox_id].num_slots; i++)
      {
         if (CurrentMbox[i] != NULL)
         {
            if (CurrentMbox[i]->status == FULL)
            {
               // this is a char offset pointer to access the mem address of the message and apply it to result
               // finding the length of the str message taken out
               char *ptr = &CurrentMbox[i]->message;
               int result = strlen(ptr) + 1;
               memcpy(msg_ptr, &(CurrentMbox[i]->message), msg_size);
               CurrentMbox[i]->message = NULL;
               CurrentMbox[i]->status = EMPTY;
               if (MailBoxTable[mbox_id].is_blocked == 1 && MailBoxTable[mbox_id].block_reason == BLOCK_SELF)
               {
                  int pid = MailBoxTable[mbox_id].pid[0];
                  shift_pids(&MailBoxTable[mbox_id]);
                  if (MailBoxTable[mbox_id].pid[0] != -1)
                  {
                     unblock_proc(pid);
                     return result;
                  }
                  else
                  {
                     MailBoxTable[mbox_id].block_reason = 0;
                     MailBoxTable[mbox_id].is_blocked = 0;
                     unblock_proc(pid);
                  }
               }
               return result;
            }
         }
      }
      // couldn't find a mailbox with a message, blocking process until it recieves a message
      MailBoxTable[mbox_id].block_reason = BLOCK_SELF;
      add_pid(&MailBoxTable[mbox_id]);
      MailBoxTable[mbox_id].is_blocked = 1;
      block_me(11);
   }

} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ----------------------------------------------------------------------- */
int MboxRelease(int mailboxID)
{
   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not release mailbox\n");
      halt(1);
   }

   // Zap all processes waiting on mailbox
} /* MboxRelease */

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ----------------------------------------------------------------------- */
int MboxCondSend(int mailboxID, void *message, int messageSize)
{
   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not conditionally send a message\n");
      halt(1);
   }
} /* MboxCondSend*/

/* ------------------------------------------------------------------------
   Name - MboxCondReceive
   Purpose -
   Parameters -
   Returns -
   Side Effects -
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mailboxID, void *message, int maxMessageSize)
{
   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not conditionally receive a message\n");
      halt(1);
   }
} /* MboxCondReceive */

/* ------------------------------------------------------------------------
  Name - check_io
  Purpose -
  Parameters -
  Returns -
 ----------------------------------------------------------------------- */
int check_io()
{
   return 0;
} /* check_io */

/* ------------------------------------------------------------------------
  Name - waitdevice
  Purpose -
  Parameters -
  Returns -
 ----------------------------------------------------------------------- */
int waitdevice(int type, int unit, int *status)
{

} /* waitdevice */

/* ------------------------------------------------------------------------
  Name - disableInterrupts
  Purpose - Disables the interrupts
  Parameters - none
  Returns - none
 ----------------------------------------------------------------------- */
void disableInterrupts()
{
   /* turn the interrupts OFF iff we are in kernel mode */
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // not in kernel mode
      console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
      halt(1);
   }
   else
      /* We ARE in kernel mode */
      psr_set(psr_get() & ~PSR_CURRENT_INT);
} /* disableInterrupts */

/* ------------------------------------------------------------------------
  Name - enableInterrupts
  Purpose - Enables the interrupts
  Parameters - none
  Returns - none
 ----------------------------------------------------------------------- */
void enableInterrupts()
{
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not enable interrupts\n");
      halt(1);
   }
   else
   {
      // KERNEL MODE
      psr_set(psr_get() | PSR_CURRENT_INT);
   }
} /* enableInterrupts */

void shift_pids(mail_box *input)
{
   mail_box *cur = input;
   for (int i = 0; i < MAXPROC; i++)
   {
      if (i + 1 > MAXPROC)
      {
         cur->pid[i] = -1;
         return;
      }
      else if (cur->pid[i] == -1)
      {
         return;
      }
      else
      {
         input->pid[i] = input->pid[i + 1];
      }
   }
}

void add_pid(mail_box *input)
{
   mail_box *cur = input;
   for (int i = 0; i < MAXPROC; i++)
   {
      if (cur->pid[i] == -1)
      {
         cur->pid[i] = getpid();
         return;
      }
   }
}