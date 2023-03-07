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
void enqueue(queue *input, slot_ptr msg_slot);
slot_ptr dequeue(queue *input);

/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* the mail boxes */
mail_box MailBoxTable[MAXMBOX];

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

      // for (int j = 0; j < MAXPROC; j++)
      // {
      //    MailBoxTable[i].pid[j] = -1;
      // }
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

   if (slots > MAXSLOTS || slots < 0)
   {
      console("Error: requested slots greater than maxium or less than minimum allowable slot number\n");
      halt(1);
   }

   if (slot_size > MAX_MESSAGE || slot_size < 0)
   {
      console("Error: requested slot size greater than maxiumum or less than minimum allowable message size\n");
   }

   // find an empty ID slot on the table
   int i = 0;
   while (1)
   {
      if (MailBoxTable[i].mbox_id == -1)
      {
         MailBoxTable[i].mbox_id = i;
         MailBoxTable[i].num_slots_remaining = slots;
         MailBoxTable[i].total_slots = slots;
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

   MailBoxTable[mailbox_id].slots.size = slot_size;
   MailBoxTable[mailbox_id].slots.type = 1; // indicate its a mail_slot
   MailBoxTable[mailbox_id].send_blocked.size = slot_size;
   MailBoxTable[mailbox_id].recieve_blocked.size = slot_size;
   MailBoxTable[mailbox_id].size_of_slots = slot_size;

   MailBoxTable[mailbox_id].slots.head = malloc(sizeof(slot_ptr));
   MailBoxTable[mailbox_id].slots.head->status = EMPTY;
   MailBoxTable[mailbox_id].slots.tail = MailBoxTable[mailbox_id].slots.head;

   MailBoxTable[mailbox_id].send_blocked.head = malloc(sizeof(slot_ptr));
   MailBoxTable[mailbox_id].send_blocked.head->status = EMPTY;
   MailBoxTable[mailbox_id].send_blocked.tail = MailBoxTable[mailbox_id].send_blocked.head;

   MailBoxTable[mailbox_id].recieve_blocked.head = malloc(sizeof(slot_ptr));
   MailBoxTable[mailbox_id].recieve_blocked.head->status = EMPTY;
   MailBoxTable[mailbox_id].recieve_blocked.tail = MailBoxTable[mailbox_id].recieve_blocked.head;
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
   slot_ptr slot_to_use = NULL;
   mail_box *CurrentMbox = &MailBoxTable[mbox_id];
   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not send message\n");
      halt(1);
   }
   // check if msg_size is greater than max allowed
   // check if mbox_id is initialized on MailBoxTable
   slot_to_use = malloc(sizeof(slot_ptr));
   memcpy(&(slot_to_use->message), msg_ptr, msg_size);
   slot_to_use->status = FULL;
   slot_to_use->mbox_id = mbox_id;
   void *ptr = &(slot_to_use->message);
   int result = strlen(ptr) + 1;
   slot_to_use->size_of_message = result;

   while (1)
   {
      if (is_zapped())
      {
         return -3;
      }
      if (CurrentMbox->num_slots_remaining > 0)
      {
         if (CurrentMbox->recieve_blocked.head->status != EMPTY)
         {
            // there is a blocked recieve, take the slot and put it onto the slots queue
            slot_ptr old_blocked = dequeue(&CurrentMbox->recieve_blocked);
            enqueue(&CurrentMbox->slots, slot_to_use);
            CurrentMbox->num_slots_remaining--;
            unblock_proc(old_blocked->blocked_pid);
         }
         else
         {
            enqueue(&CurrentMbox->slots, slot_to_use);
            CurrentMbox->num_slots_remaining--;
         }
         free(slot_to_use);
         return 0;
         // there is an avalible mail_slot open
      }
      else
      {
         enqueue(&CurrentMbox->send_blocked, slot_to_use);
         block_me(11);
      }
      free(slot_to_use);
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
   mail_box *CurrentMbox = &MailBoxTable[mbox_id];

   // check if in kernel mode
   if ((PSR_CURRENT_MODE & psr_get()) == 0)
   {
      // IN USER MODE
      console("Kernel Error: Not in kernel mode, may not send message\n");
      halt(1);
   }
   // check if msg_size is greater than max allowed
   // check if mbox_id is initialized on MailBoxTable
   slot_ptr oldSlot;
   while (1)
   {
      slot_ptr messageSlot;
      if (is_zapped())
      {
         return -3;
      }
      if (CurrentMbox->num_slots_remaining != CurrentMbox->total_slots)
      {
         if (CurrentMbox->slots.head->status != EMPTY)
         {
            CurrentMbox->num_slots_remaining++;
            messageSlot = dequeue(&CurrentMbox->slots);
            memcpy(msg_ptr, &(messageSlot->message), msg_size);
         }
         else if (CurrentMbox->send_blocked.head->status != EMPTY)
         {
            messageSlot = dequeue(&CurrentMbox->send_blocked);
            memcpy(msg_ptr, &(messageSlot->message), msg_size);
            unblock_proc(messageSlot->blocked_pid);
         }
         int size = messageSlot->size_of_message;
         messageSlot->status = EMPTY;
         messageSlot->message = NULL;
         messageSlot->blocked_pid = -1;
         messageSlot->size_of_message = 0;
         return (size);
         // there are messages
      }
      else
      {
         messageSlot->status = FULL;
         messageSlot->blocked_pid = getpid();
         enqueue(&CurrentMbox->recieve_blocked, messageSlot);
         block_me(11);
      }
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

void enqueue(queue *input, slot_ptr msg_slot)
{
   queue *Current = input;
   slot_ptr message = Current->head;
   if (Current->head->status == EMPTY)
   {
      message->mbox_id = msg_slot->mbox_id;
      message->size_of_message = msg_slot->size_of_message;
      message->status = FULL;
      message->next_slot = malloc(sizeof(slot_ptr));
      message->next_slot->status = EMPTY;
      memcpy(&(message->message), &(msg_slot->message), msg_slot->size_of_message);
      message->blocked_pid = msg_slot->blocked_pid;
   }
   else
   {
      slot_ptr OldSlot = Current->head->next_slot;
      while (OldSlot->status != EMPTY)
      {
         OldSlot = OldSlot->next_slot;
      }
      OldSlot->mbox_id = msg_slot->mbox_id;
      OldSlot->size_of_message = msg_slot->size_of_message;
      OldSlot->status = FULL;
      OldSlot->blocked_pid = msg_slot->blocked_pid;
      OldSlot->next_slot = malloc(sizeof(slot_ptr));
      OldSlot->next_slot->status = EMPTY;
      memcpy(&(OldSlot->message), &(msg_slot->message), msg_slot->size_of_message);
   }
}

slot_ptr dequeue(queue *input)
{
   queue *Current = input;
   slot_ptr check = Current->head;
   // find a message that is not empty in the queue
   while (check->status != FULL)
   {
      check = check->next_slot;
   }
   check->status = EMPTY;
   return check;
}