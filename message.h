#define DEBUG2 1

typedef struct mail_slot *slot_ptr;
typedef struct mailbox mail_box;
typedef struct mbox_proc *mbox_proc_ptr;

struct mailbox {
   int           mbox_id;
   /* other items as needed... */
   slot_ptr      mail_slot_array[MAXSLOTS]; // -101 == DO NOT USE, -100 == OPEN FOR USE
   int           num_slots;
   int           is_blocked;
   int           block_reason;
   int           pid[MAXPROC];
};

struct mail_slot {
   int       mbox_id;
   int       status;
   /* other items as needed... */
   void      *message;
   int       mail_slot_id;
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
