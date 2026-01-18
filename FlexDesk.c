#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define MEMBERS_FILE "members.csv"
#define WORKSPACES_FILE "workspaces.csv"
#define BOOKINGS_FILE "bookings.csv"
#define PAYMENTS_FILE "payments.csv"
#define LOG_FILE "system.log"

// HASH MAP CONFIGURATION
#define INDEX_SIZE 1009 // Prime number to reduce collisions

/* * ==========================================
 * DATA STRUCTURES
 * ==========================================
 */

typedef struct
{
    int memberId;
    char name[100];
    char email[100];
} Member;

typedef struct
{
    int workspaceId;
    char type[50];
    char location[100];
    int capacity;
    int price_in_cents;
} Workspace;

typedef struct
{
    int bookingId;
    int memberId;
    int workspaceId;
    char startTime[20];
    char endTime[20];
    char status[20];
} Booking;

typedef struct
{
    int paymentId;
    int bookingId;
    int amount_in_cents;
    char paymentDate[11];
    char status[20];
} Payment;

// -- Doubly Linked List Nodes (Storage) --
typedef struct MemberNode
{
    Member data;
    struct MemberNode *next, *prev;
} MemberNode;

typedef struct WorkspaceNode
{
    Workspace data;
    struct WorkspaceNode *next, *prev;
} WorkspaceNode;

typedef struct BookingNode
{
    Booking data;
    struct BookingNode *next, *prev;
} BookingNode;

typedef struct PaymentNode
{
    Payment data;
    struct PaymentNode *next, *prev;
} PaymentNode;

// -- Index Nodes (Lookup) --
// This structure lives in the Hash Map buckets.
// It points TO the actual data in the main linked list.
typedef struct IndexNode
{
    MemberNode *target;     // Pointer to the actual data node
    struct IndexNode *next; // Chaining for collisions
} IndexNode;

// -- Global List Pointers --
MemberNode *member_head = NULL, *member_tail = NULL;
WorkspaceNode *workspace_head = NULL, *workspace_tail = NULL;
BookingNode *booking_head = NULL, *booking_tail = NULL;
PaymentNode *payment_head = NULL, *payment_tail = NULL;

// -- Global Hash Map --
IndexNode *member_index[INDEX_SIZE]; // Array of pointers (Buckets)

/* * ==========================================
 * CONCURRENCY CONTROL
 * ==========================================
 */
pthread_rwlock_t members_lock, workspaces_lock, bookings_lock, payments_lock;
pthread_mutex_t log_mutex;

int next_member_id = 1, next_workspace_id = 1, next_booking_id = 1, next_payment_id = 1;

// -- Forward Declarations --
void load_all_data();
void save_all_data();
void free_all_lists();
void log_operation(const char *message);

// Indexing Functions
int hash_function(int id);
void add_to_index(MemberNode *node);
void remove_from_index(int id);
void free_index();

// CRUD Prototypes
void addMember();
void displayAllMembers();
void updateMember();
void deleteMember();
MemberNode *findMemberNodeById(int id);

void addWorkspace();
void displayAllWorkspaces();
void updateWorkspace();
void deleteWorkspace();
WorkspaceNode *findWorkspaceNodeById(int id);

void addBooking();
void displayAllBookings();
void updateBooking();
void deleteBooking();
BookingNode *findBookingNodeById(int id);

void addPayment();
void displayAllPayments();
void updatePayment();
void deletePayment();
PaymentNode *findPaymentNodeById(int id);

void run_concurrency_test();

int main()
{
    // 1. Initialize Locks
    pthread_rwlock_init(&members_lock, NULL);
    pthread_rwlock_init(&workspaces_lock, NULL);
    pthread_rwlock_init(&bookings_lock, NULL);
    pthread_rwlock_init(&payments_lock, NULL);
    pthread_mutex_init(&log_mutex, NULL);

    // 2. Initialize Hash Map
    for(int i = 0; i < INDEX_SIZE; i++) member_index[i] = NULL;

    // 3. Load initial state
    log_operation("System Started");
    load_all_data();

    int choice = 0;
    while (1)
    {
        printf("\n========================================\n");
        printf("  Co-Working Space DBMS (Indexed)\n");
        printf("========================================\n");
        printf("--- Members ---\n");
        printf("  1. Add Member        2. Display Members\n");
        printf("  3. Update Member     4. Delete Member\n");
        printf("--- Workspaces ---\n");
        printf("  5. Add Workspace     6. Display Workspaces\n");
        printf("  7. Update Workspace  8. Delete Workspace\n");
        printf("--- Bookings ---\n");
        printf("  9. Add Booking       10. Display Bookings\n");
        printf("  11. Update Booking   12. Delete Booking\n");
        printf("--- Payments ---\n");
        printf("  13. Add Payment      14. Display Payments\n");
        printf("  15. Update Payment   16. Delete Payment\n");
        printf("----------------------------------------\n");
        printf("  88. RUN CONCURRENCY TEST (Demo)\n");
        printf("  99. Save & Exit\n");
        printf("========================================\n");
        printf("> ");

        char input[10];
        if (fgets(input, sizeof(input), stdin) == NULL)
            break;
        if (sscanf(input, "%d", &choice) != 1)
            choice = 0;

        switch (choice)
        {
        case 1: addMember(); break;
        case 2: displayAllMembers(); break;
        case 3: updateMember(); break;
        case 4: deleteMember(); break;
        case 5: addWorkspace(); break;
        case 6: displayAllWorkspaces(); break;
        case 7: updateWorkspace(); break;
        case 8: deleteWorkspace(); break;
        case 9: addBooking(); break;
        case 10: displayAllBookings(); break;
        case 11: updateBooking(); break;
        case 12: deleteBooking(); break;
        case 13: addPayment(); break;
        case 14: displayAllPayments(); break;
        case 15: updatePayment(); break;
        case 16: deletePayment(); break;

        case 88:
            run_concurrency_test();
            break;

        case 99:
            save_all_data();
            free_all_lists();
            free_index(); // Clean up index memory
            log_operation("System Shutdown");
            printf("All data saved. Exiting ...\n");

            pthread_rwlock_destroy(&members_lock);
            pthread_rwlock_destroy(&workspaces_lock);
            pthread_rwlock_destroy(&bookings_lock);
            pthread_rwlock_destroy(&payments_lock);
            pthread_mutex_destroy(&log_mutex);
            return 0;
        default:
            printf("Invalid choice. Please try again.\n");
        }
    }
    return 0;
}

//  HELPER & GENERIC FUNCTIONS

void getString(const char *prompt, char *buffer, int size)
{
    printf("%s", prompt);
    fgets(buffer, size, stdin);
    buffer[strcspn(buffer, "\n")] = 0;
}

int getInt(const char *prompt)
{
    int value;
    printf("%s", prompt);
    while (scanf("%d", &value) != 1)
    {
        while (getchar() != '\n');
        printf("Invalid input. Please enter a number: ");
    }
    while (getchar() != '\n');
    return value;
}

void log_operation(const char *message)
{
    pthread_mutex_lock(&log_mutex);
    FILE *f = fopen(LOG_FILE, "a");
    if (f)
    {
        time_t now = time(NULL);
        char *t_str = ctime(&now);
        t_str[strcspn(t_str, "\n")] = 0;
        fprintf(f, "[%s] LOG: %s\n", t_str, message);
        fclose(f);
    }
    pthread_mutex_unlock(&log_mutex);
}

/* * ==========================================
 * HASH MAP IMPLEMENTATION
 * ==========================================
 */

// Simple Modulo Hash
int hash_function(int id) {
    return id % INDEX_SIZE;
}

// Add a MemberNode pointer to the index
void add_to_index(MemberNode *node) {
    if (!node) return;
    int index = hash_function(node->data.memberId);

    IndexNode *newIndexNode = (IndexNode*)malloc(sizeof(IndexNode));
    newIndexNode->target = node;

    // Insert at head of the bucket (Chain)
    newIndexNode->next = member_index[index];
    member_index[index] = newIndexNode;
}

// Remove an entry from the index
void remove_from_index(int id) {
    int index = hash_function(id);
    IndexNode *current = member_index[index];
    IndexNode *prev = NULL;

    while (current != NULL) {
        if (current->target->data.memberId == id) {
            // Found it
            if (prev == NULL) {
                // Head of bucket
                member_index[index] = current->next;
            } else {
                // Middle of bucket
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Free all index memory on exit
void free_index() {
    for (int i = 0; i < INDEX_SIZE; i++) {
        IndexNode *curr = member_index[i];
        while (curr) {
            IndexNode *temp = curr;
            curr = curr->next;
            free(temp);
        }
    }
}

/* * ==========================================
 * MEMBER FUNCTIONS (Updated with Indexing)
 * ==========================================
 */

// O(1) Lookup - The "Next Level" Upgrade
MemberNode *findMemberNodeById(int id)
{
    int index = hash_function(id);
    IndexNode *curr = member_index[index];

    // Traverse the bucket (usually 1 or 2 items max)
    while (curr != NULL) {
        if (curr->target->data.memberId == id) {
            return curr->target;
        }
        curr = curr->next;
    }
    // Note: No linear search of member_head needed anymore!
    return NULL;
}

void addMember()
{
    MemberNode *newNode = (MemberNode *)malloc(sizeof(MemberNode));
    newNode->next = newNode->prev = NULL;
    getString("Enter name: ", newNode->data.name, 100);
    getString("Enter email: ", newNode->data.email, 100);

    pthread_rwlock_wrlock(&members_lock);

    // Check duplicate email (still O(N) unless we add a second index for email!)
    MemberNode *curr = member_head;
    int emailExists = 0;
    while(curr != NULL) {
        if(strcmp(curr->data.email, newNode->data.email) == 0) {
            emailExists = 1;
            break;
        }
        curr = curr->next;
    }

    if (emailExists) {
        printf("Error: Email already exists.\n");
        free(newNode);
        pthread_rwlock_unlock(&members_lock);
        return;
    }

    newNode->data.memberId = next_member_id++;

    // 1. Add to Main List (Storage)
    if (!member_head)
        member_head = member_tail = newNode;
    else
    {
        member_tail->next = newNode;
        newNode->prev = member_tail;
        member_tail = newNode;
    }

    // 2. Add to Hash Index (Lookup)
    add_to_index(newNode);

    char logMsg[150];
    sprintf(logMsg, "Added Member ID %d (%s)", newNode->data.memberId, newNode->data.name);
    log_operation(logMsg);

    pthread_rwlock_unlock(&members_lock);
    printf("Member added with ID %d.\n", newNode->data.memberId);
}

void displayAllMembers()
{
    printf("\n--- All Members ---\n%-5s | %-30s | %-30s\n", "ID", "Name", "Email");
    printf("------|--------------------------------|--------------------------------\n");

    pthread_rwlock_rdlock(&members_lock);
    // Iterate the main list for display (Indices are not good for iteration)
    for (MemberNode *curr = member_head; curr != NULL; curr = curr->next)
    {
        printf("%-5d | %-30s | %-30s\n", curr->data.memberId, curr->data.name, curr->data.email);
    }
    pthread_rwlock_unlock(&members_lock);
}

void updateMember()
{
    int id = getInt("Enter ID of member to update: ");

    pthread_rwlock_wrlock(&members_lock);
    MemberNode *node = findMemberNodeById(id); // Uses Index O(1)
    if (node)
    {
        printf("Updating Member ID %d (Name: %s)\n", id, node->data.name);
        getString("Enter new name (or Enter to skip): ", node->data.name, 100);

        char logMsg[100];
        sprintf(logMsg, "Updated Member ID %d", id);
        log_operation(logMsg);
        printf("Member ID %d updated.\n", id);
    }
    else
        printf("Member not found.\n");
    pthread_rwlock_unlock(&members_lock);
}

void deleteMember()
{
    int id = getInt("Enter ID of member to delete: ");

    pthread_rwlock_wrlock(&members_lock);
    MemberNode *node = findMemberNodeById(id); // Uses Index O(1)
    if (node)
    {
        // 1. Remove from Main List
        if (node->prev)
            node->prev->next = node->next;
        else
            member_head = node->next;
        if (node->next)
            node->next->prev = node->prev;
        else
            member_tail = node->prev;

        // 2. Remove from Index
        remove_from_index(id);

        free(node);

        char logMsg[100];
        sprintf(logMsg, "Deleted Member ID %d", id);
        log_operation(logMsg);
        printf("Member ID %d deleted.\n", id);
    }
    else
        printf("Member not found.\n");
    pthread_rwlock_unlock(&members_lock);
}

/* * ==========================================
 * OTHER FUNCTIONS (Unchanged, but they benefit from Indexing indirectly via bookings)
 * ==========================================
 */

WorkspaceNode *findWorkspaceNodeById(int id)
{
    WorkspaceNode *current = workspace_head;
    while (current != NULL)
    {
        if (current->data.workspaceId == id)
            return current;
        current = current->next;
    }
    return NULL;
}

void addWorkspace()
{
    WorkspaceNode *newNode = (WorkspaceNode *)malloc(sizeof(WorkspaceNode));
    newNode->next = newNode->prev = NULL;
    getString("Enter type: ", newNode->data.type, 50);
    getString("Enter location: ", newNode->data.location, 100);
    newNode->data.capacity = getInt("Enter capacity: ");
    newNode->data.price_in_cents = getInt("Enter price (in cents): ");

    pthread_rwlock_wrlock(&workspaces_lock);
    newNode->data.workspaceId = next_workspace_id++;
    if (!workspace_head)
        workspace_head = workspace_tail = newNode;
    else
    {
        workspace_tail->next = newNode;
        newNode->prev = workspace_tail;
        workspace_tail = newNode;
    }

    char logMsg[150];
    sprintf(logMsg, "Added Workspace ID %d (%s)", newNode->data.workspaceId, newNode->data.type);
    log_operation(logMsg);

    pthread_rwlock_unlock(&workspaces_lock);
    printf("Workspace added with ID %d.\n", newNode->data.workspaceId);
}

void displayAllWorkspaces()
{
    printf("\n--- All Workspaces ---\n%-5s | %-20s | %-20s | %-10s | %s\n", "ID", "Type", "Location", "Capacity", "Price(cents)");
    printf("------|----------------------|----------------------|------------|-------------\n");

    pthread_rwlock_rdlock(&workspaces_lock);
    for (WorkspaceNode *curr = workspace_head; curr != NULL; curr = curr->next)
    {
        printf("%-5d | %-20s | %-20s | %-10d | %d\n", curr->data.workspaceId, curr->data.type, curr->data.location, curr->data.capacity, curr->data.price_in_cents);
    }
    pthread_rwlock_unlock(&workspaces_lock);
}

void updateWorkspace()
{
    int id = getInt("Enter ID of workspace to update: ");

    pthread_rwlock_wrlock(&workspaces_lock);
    WorkspaceNode *node = findWorkspaceNodeById(id);
    if (node)
    {
        printf("Updating Workspace ID %d (Type: %s)\n", id, node->data.type);
        node->data.capacity = getInt("Enter new capacity: ");
        node->data.price_in_cents = getInt("Enter new price (in cents): ");

        char logMsg[100];
        sprintf(logMsg, "Updated Workspace ID %d", id);
        log_operation(logMsg);
        printf("Workspace ID %d updated.\n", id);
    }
    else
        printf("Workspace not found.\n");
    pthread_rwlock_unlock(&workspaces_lock);
}

void deleteWorkspace()
{
    int id = getInt("Enter ID of workspace to delete: ");

    pthread_rwlock_wrlock(&workspaces_lock);
    WorkspaceNode *node = findWorkspaceNodeById(id);
    if (node)
    {
        if (node->prev)
            node->prev->next = node->next;
        else
            workspace_head = node->next;
        if (node->next)
            node->next->prev = node->prev;
        else
            workspace_tail = node->prev;
        free(node);

        char logMsg[100];
        sprintf(logMsg, "Deleted Workspace ID %d", id);
        log_operation(logMsg);
        printf("Workspace ID %d deleted.\n", id);
    }
    else
        printf("Workspace not found.\n");
    pthread_rwlock_unlock(&workspaces_lock);
}

//  BOOKING FUNCTIONS

BookingNode *findBookingNodeById(int id)
{
    BookingNode *current = booking_head;
    while (current != NULL)
    {
        if (current->data.bookingId == id)
            return current;
        current = current->next;
    }
    return NULL;
}

void addBooking()
{
    int mId = getInt("Enter Member ID: ");
    int wId = getInt("Enter Workspace ID: ");

    // INTEGRITY CHECK: Verify Member Exists
    // Now uses Hash Map Index (Fast Lookup)
    pthread_rwlock_rdlock(&members_lock);
    MemberNode *mCheck = findMemberNodeById(mId);
    if (mCheck == NULL) {
        printf("Error: Member ID %d does not exist. Cannot create booking.\n", mId);
        pthread_rwlock_unlock(&members_lock);
        return;
    }
    pthread_rwlock_unlock(&members_lock);

    // INTEGRITY CHECK: Verify Workspace Exists
    pthread_rwlock_rdlock(&workspaces_lock);
    WorkspaceNode *wCheck = findWorkspaceNodeById(wId);
    if (wCheck == NULL) {
        printf("Error: Workspace ID %d does not exist. Cannot create booking.\n", wId);
        pthread_rwlock_unlock(&workspaces_lock);
        return;
    }
    pthread_rwlock_unlock(&workspaces_lock);

    // If we passed checks, proceed to creation
    BookingNode *newNode = (BookingNode *)malloc(sizeof(BookingNode));
    newNode->next = newNode->prev = NULL;
    newNode->data.memberId = mId;
    newNode->data.workspaceId = wId;
    getString("Enter Start Time (YYYY-MM-DDTHH:MM): ", newNode->data.startTime, 20);
    getString("Enter End Time (YYYY-MM-DDTHH:MM): ", newNode->data.endTime, 20);
    getString("Enter Status (e.g., Confirmed): ", newNode->data.status, 20);

    pthread_rwlock_wrlock(&bookings_lock);
    newNode->data.bookingId = next_booking_id++;
    if (!booking_head)
        booking_head = booking_tail = newNode;
    else
    {
        booking_tail->next = newNode;
        newNode->prev = booking_tail;
        booking_tail = newNode;
    }

    char logMsg[150];
    sprintf(logMsg, "Added Booking ID %d (Mem: %d, WS: %d)", newNode->data.bookingId, mId, wId);
    log_operation(logMsg);

    pthread_rwlock_unlock(&bookings_lock);
    printf("Booking added with ID %d.\n", newNode->data.bookingId);
}

void displayAllBookings()
{
    printf("\n--- All Bookings ---\n%-5s | %-10s | %-12s | %-18s | %-18s | %s\n", "ID", "Member ID", "Workspace ID", "Start Time", "End Time", "Status");
    printf("------|------------|--------------|--------------------|--------------------|----------\n");

    pthread_rwlock_rdlock(&bookings_lock);
    for (BookingNode *curr = booking_head; curr != NULL; curr = curr->next)
    {
        printf("%-5d | %-10d | %-12d | %-18s | %-18s | %s\n", curr->data.bookingId, curr->data.memberId, curr->data.workspaceId, curr->data.startTime, curr->data.endTime, curr->data.status);
    }
    pthread_rwlock_unlock(&bookings_lock);
}

void updateBooking()
{
    int id = getInt("Enter ID of booking to update: ");

    pthread_rwlock_wrlock(&bookings_lock);
    BookingNode *node = findBookingNodeById(id);
    if (node)
    {
        printf("Updating Booking ID %d. Current status: %s\n", id, node->data.status);
        getString("Enter new status (e.g., Cancelled): ", node->data.status, 20);

        char logMsg[100];
        sprintf(logMsg, "Updated Booking ID %d status to %s", id, node->data.status);
        log_operation(logMsg);
        printf("Booking ID %d updated.\n", id);
    }
    else
        printf("Booking not found.\n");
    pthread_rwlock_unlock(&bookings_lock);
}

void deleteBooking()
{
    int id = getInt("Enter ID of booking to delete: ");

    pthread_rwlock_wrlock(&bookings_lock);
    BookingNode *node = findBookingNodeById(id);
    if (node)
    {
        if (node->prev)
            node->prev->next = node->next;
        else
            booking_head = node->next;
        if (node->next)
            node->next->prev = node->prev;
        else
            booking_tail = node->prev;
        free(node);

        char logMsg[100];
        sprintf(logMsg, "Deleted Booking ID %d", id);
        log_operation(logMsg);
        printf("Booking ID %d deleted.\n", id);
    }
    else
        printf("Booking not found.\n");
    pthread_rwlock_unlock(&bookings_lock);
}

//  PAYMENT FUNCTIONS

PaymentNode *findPaymentNodeById(int id)
{
    PaymentNode *current = payment_head;
    while (current != NULL)
    {
        if (current->data.paymentId == id)
            return current;
        current = current->next;
    }
    return NULL;
}

void addPayment()
{
    int bId = getInt("Enter Booking ID: ");

    // INTEGRITY CHECK: Verify Booking Exists
    pthread_rwlock_rdlock(&bookings_lock);
    BookingNode *bCheck = findBookingNodeById(bId);
    if (bCheck == NULL) {
        printf("Error: Booking ID %d does not exist. Cannot process payment.\n", bId);
        pthread_rwlock_unlock(&bookings_lock);
        return;
    }
    pthread_rwlock_unlock(&bookings_lock);

    PaymentNode *newNode = (PaymentNode *)malloc(sizeof(PaymentNode));
    newNode->next = newNode->prev = NULL;
    newNode->data.bookingId = bId;
    newNode->data.amount_in_cents = getInt("Enter amount (in cents): ");
    getString("Enter Payment Date (YYYY-MM-DD): ", newNode->data.paymentDate, 11);
    getString("Enter Status (e.g., Paid): ", newNode->data.status, 20);

    pthread_rwlock_wrlock(&payments_lock);
    newNode->data.paymentId = next_payment_id++;
    if (!payment_head)
        payment_head = payment_tail = newNode;
    else
    {
        payment_tail->next = newNode;
        newNode->prev = payment_tail;
        payment_tail = newNode;
    }

    char logMsg[150];
    sprintf(logMsg, "Added Payment ID %d for Booking %d", newNode->data.paymentId, bId);
    log_operation(logMsg);

    pthread_rwlock_unlock(&payments_lock);
    printf("Payment added with ID %d.\n", newNode->data.paymentId);
}

void displayAllPayments()
{
    printf("\n--- All Payments ---\n%-5s | %-10s | %-15s | %-12s | %s\n", "ID", "Booking ID", "Amount (cents)", "Date", "Status");
    printf("------|------------|-----------------|--------------|----------\n");

    pthread_rwlock_rdlock(&payments_lock);
    for (PaymentNode *curr = payment_head; curr != NULL; curr = curr->next)
    {
        printf("%-5d | %-10d | %-15d | %-12s | %s\n", curr->data.paymentId, curr->data.bookingId, curr->data.amount_in_cents, curr->data.paymentDate, curr->data.status);
    }
    pthread_rwlock_unlock(&payments_lock);
}

void updatePayment()
{
    int id = getInt("Enter ID of payment to update: ");

    pthread_rwlock_wrlock(&payments_lock);
    PaymentNode *node = findPaymentNodeById(id);
    if (node)
    {
        printf("Updating Payment ID %d. Current status: %s\n", id, node->data.status);
        getString("Enter new status (e.g., Refunded): ", node->data.status, 20);

        char logMsg[100];
        sprintf(logMsg, "Updated Payment ID %d status", id);
        log_operation(logMsg);
        printf("Payment ID %d updated.\n", id);
    }
    else
        printf("Payment not found.\n");
    pthread_rwlock_unlock(&payments_lock);
}

void deletePayment()
{
    int id = getInt("Enter ID of payment to delete: ");

    pthread_rwlock_wrlock(&payments_lock);
    PaymentNode *node = findPaymentNodeById(id);
    if (node)
    {
        if (node->prev)
            node->prev->next = node->next;
        else
            payment_head = node->next;
        if (node->next)
            node->next->prev = node->prev;
        else
            payment_tail = node->prev;
        free(node);

        char logMsg[100];
        sprintf(logMsg, "Deleted Payment ID %d", id);
        log_operation(logMsg);
        printf("Payment ID %d deleted.\n", id);
    }
    else
        printf("Payment not found.\n");
    pthread_rwlock_unlock(&payments_lock);
}

/*
 * ==========================================
 * FILE I/O & CONCURRENCY DEMO
 * ==========================================
 */

void load_all_data()
{
    FILE *file;
    // Load Members (Updated to populate INDEX)
    file = fopen(MEMBERS_FILE, "r");
    if (file)
    {
        Member temp;
        int maxId = 0;
        while (fscanf(file, "%d,%99[^,],%99[^\n]\n", &temp.memberId, temp.name, temp.email) == 3)
        {
            MemberNode *newNode = (MemberNode *)malloc(sizeof(MemberNode));
            newNode->data = temp;
            newNode->next = newNode->prev = NULL;
            if (!member_head)
                member_head = member_tail = newNode;
            else
            {
                member_tail->next = newNode;
                newNode->prev = member_tail;
                member_tail = newNode;
            }

            // Populating Index during load
            add_to_index(newNode);

            if (temp.memberId > maxId)
                maxId = temp.memberId;
        }
        next_member_id = maxId + 1;
        fclose(file);
    }
    // Load Workspaces
    file = fopen(WORKSPACES_FILE, "r");
    if (file)
    {
        Workspace temp;
        int maxId = 0;
        while (fscanf(file, "%d,%49[^,],%99[^,],%d,%d\n", &temp.workspaceId, temp.type, temp.location, &temp.capacity, &temp.price_in_cents) == 5)
        {
            WorkspaceNode *newNode = (WorkspaceNode *)malloc(sizeof(WorkspaceNode));
            newNode->data = temp;
            newNode->next = newNode->prev = NULL;
            if (!workspace_head)
                workspace_head = workspace_tail = newNode;
            else
            {
                workspace_tail->next = newNode;
                newNode->prev = workspace_tail;
                workspace_tail = newNode;
            }
            if (temp.workspaceId > maxId)
                maxId = temp.workspaceId;
        }
        next_workspace_id = maxId + 1;
        fclose(file);
    }
    // Load Bookings
    file = fopen(BOOKINGS_FILE, "r");
    if (file)
    {
        Booking temp;
        int maxId = 0;
        while (fscanf(file, "%d,%d,%d,%19[^,],%19[^,],%19[^\n]\n", &temp.bookingId, &temp.memberId, &temp.workspaceId, temp.startTime, temp.endTime, temp.status) == 6)
        {
            BookingNode *newNode = (BookingNode *)malloc(sizeof(BookingNode));
            newNode->data = temp;
            newNode->next = newNode->prev = NULL;
            if (!booking_head)
                booking_head = booking_tail = newNode;
            else
            {
                booking_tail->next = newNode;
                newNode->prev = booking_tail;
                booking_tail = newNode;
            }
            if (temp.bookingId > maxId)
                maxId = temp.bookingId;
        }
        next_booking_id = maxId + 1;
        fclose(file);
    }
    // Load Payments
    file = fopen(PAYMENTS_FILE, "r");
    if (file)
    {
        Payment temp;
        int maxId = 0;
        while (fscanf(file, "%d,%d,%d,%10[^,],%19[^\n]\n", &temp.paymentId, &temp.bookingId, &temp.amount_in_cents, temp.paymentDate, temp.status) == 5)
        {
            PaymentNode *newNode = (PaymentNode *)malloc(sizeof(PaymentNode));
            newNode->data = temp;
            newNode->next = newNode->prev = NULL;
            if (!payment_head)
                payment_head = payment_tail = newNode;
            else
            {
                payment_tail->next = newNode;
                newNode->prev = payment_tail;
                payment_tail = newNode;
            }
            if (temp.paymentId > maxId)
                maxId = temp.paymentId;
        }
        next_payment_id = maxId + 1;
        fclose(file);
    }
    printf("All data loaded from files.\n");
}

void save_all_data()
{
    FILE *file;

    // Save Members
    file = fopen(MEMBERS_FILE, "w");
    if (file)
    {
        pthread_rwlock_rdlock(&members_lock);
        for (MemberNode *curr = member_head; curr != NULL; curr = curr->next)
        {
            fprintf(file, "%d,%s,%s\n", curr->data.memberId, curr->data.name, curr->data.email);
        }
        pthread_rwlock_unlock(&members_lock);
        fclose(file);
    }
    // Save Workspaces
    file = fopen(WORKSPACES_FILE, "w");
    if (file)
    {
        pthread_rwlock_rdlock(&workspaces_lock);
        for (WorkspaceNode *curr = workspace_head; curr != NULL; curr = curr->next)
        {
            fprintf(file, "%d,%s,%s,%d,%d\n", curr->data.workspaceId, curr->data.type, curr->data.location, curr->data.capacity, curr->data.price_in_cents);
        }
        pthread_rwlock_unlock(&workspaces_lock);
        fclose(file);
    }
    // Save Bookings
    file = fopen(BOOKINGS_FILE, "w");
    if (file)
    {
        pthread_rwlock_rdlock(&bookings_lock);
        for (BookingNode *curr = booking_head; curr != NULL; curr = curr->next)
        {
            fprintf(file, "%d,%d,%d,%s,%s,%s\n", curr->data.bookingId, curr->data.memberId, curr->data.workspaceId, curr->data.startTime, curr->data.endTime, curr->data.status);
        }
        pthread_rwlock_unlock(&bookings_lock);
        fclose(file);
    }
    // Save Payments
    file = fopen(PAYMENTS_FILE, "w");
    if (file)
    {
        pthread_rwlock_rdlock(&payments_lock);
        for (PaymentNode *curr = payment_head; curr != NULL; curr = curr->next)
        {
            fprintf(file, "%d,%d,%d,%s,%s\n", curr->data.paymentId, curr->data.bookingId, curr->data.amount_in_cents, curr->data.paymentDate, curr->data.status);
        }
        pthread_rwlock_unlock(&payments_lock);
        fclose(file);
    }
}

void free_all_lists()
{
    MemberNode *currentMember, *tmpMember;
    currentMember = member_head;
    while (currentMember)
    {
        tmpMember = currentMember;
        currentMember = currentMember->next;
        free(tmpMember);
    }
    WorkspaceNode *currentWorkspace, *tmpWorkspace;
    currentWorkspace = workspace_head;
    while (currentWorkspace)
    {
        tmpWorkspace = currentWorkspace;
        currentWorkspace = currentWorkspace->next;
        free(tmpWorkspace);
    }
    BookingNode *currentBooking, *tmpBooking;
    currentBooking = booking_head;
    while (currentBooking)
    {
        tmpBooking = currentBooking;
        currentBooking = currentBooking->next;
        free(tmpBooking);
    }
    PaymentNode *currentPayment, *tmpPayment;
    currentPayment = payment_head;
    while (currentPayment)
    {
        tmpPayment = currentPayment;
        currentPayment = currentPayment->next;
        free(tmpPayment);
    }
}

// Demo functions for concurrency (Reader/Writer)
void *demo_reader(void *arg) {
    int id = *(int*)arg;
    printf("[Thread R%d] Requesting READ lock...\n", id);

    pthread_rwlock_rdlock(&members_lock);
    printf("   [Thread R%d] GRANTED Read Lock. Reading database...\n", id);
    sleep(2);
    printf("   [Thread R%d] Done reading. Releasing lock.\n", id);

    pthread_rwlock_unlock(&members_lock);
    return NULL;
}

void *demo_writer(void *arg) {
    int id = *(int*)arg;
    printf("[Thread W%d] Requesting WRITE lock (Exclusive)...\n", id);

    pthread_rwlock_wrlock(&members_lock);
    printf("   >>> [Thread W%d] GRANTED Write Lock. Modifying database... <<<\n", id);
    sleep(2);
    printf("   >>> [Thread W%d] Done writing. Releasing lock. <<<\n", id);

    pthread_rwlock_unlock(&members_lock);
    return NULL;
}

void run_concurrency_test() {
    printf("\n--- Starting Concurrency Stress Test ---\n");
    printf("Goal: Show that Readers can overlap, but Writers block everyone.\n");
    printf("1. Launching Reader 1\n");
    printf("2. Launching Reader 2 (Should start immediately - overlapping R1)\n");
    printf("3. Launching Writer 1 (Should WAIT until Readers finish)\n\n");

    pthread_t r1, r2, w1;
    int id1 = 1, id2 = 2, id3 = 1;

    pthread_create(&r1, NULL, demo_reader, &id1);
    usleep(100000);

    pthread_create(&r2, NULL, demo_reader, &id2);
    usleep(100000);

    pthread_create(&w1, NULL, demo_writer, &id3);

    pthread_join(r1, NULL);
    pthread_join(r2, NULL);
    pthread_join(w1, NULL);

    printf("\n--- Test Complete: Check output order above ---\n");
}
