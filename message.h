#define DEBUG2 1

typedef struct mail_slot *slot_ptr;
typedef struct mail_slot slot_not_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;
typedef struct queue queue;

struct queue {
   slot_ptr head;
   slot_ptr tail;
   int      size;
   int      type;
   int      blocked_pid;
};

struct mailbox {
   int           mbox_id;
   /* other items as needed... */
   //slot_ptr      mail_slot_array[MAXSLOTS]; // -101 == DO NOT USE, -100 == OPEN FOR USE
   int           num_slots_remaining;
   // int           is_blocked;
   // int           block_reason;
   // int           pid[MAXPROC];
   int           total_slots;
   int           size_of_slots;
   queue         slots;
   queue         send_blocked;
   queue         recieve_blocked;
};

struct mail_slot {
   int       mbox_id;
   int       status;
   slot_ptr  next_slot;
   /* other items as needed... */
   void      *message;
   int       msg_overflow[MAX_MESSAGE];
   int       size_of_message;
   int       blocked_pid;
};

struct psr_bits {
    unsigned int cur_mode;
    unsigned int cur_int_enable:1;
    unsigned int prev_mode:1;
    unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};



#define EMPTY 1
#define FULL  2

#define BLOCK_SELF 101
#define BLOCK_ZAP  102

#define DO_NOT_USE -101
