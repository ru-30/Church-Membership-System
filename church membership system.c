#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ------------------------------ CONSTANTS ------------------------------ */
#define MAX_MEMBERS       100      /* capacity of the member array (>= 50)   */
#define QUEUE_CAPACITY    20       /* capacity of the service-request queue  */
#define STACK_CAPACITY    50       /* capacity of the modification stack     */
#define LINE_LEN          256      /* size of general input buffers          */

#define ID_LEN            16
#define NAME_LEN          30
#define FULLNAME_LEN      62
#define GENDER_LEN        10
#define PHONE_LEN         17
#define EMAIL_LEN         50
#define DATE_LEN          11       /* YYYY-MM-DD + '\0'                      */
#define MINISTRY_LEN      20
#define STATUS_LEN        12
#define TYPE_LEN          16
#define DESC_LEN          100
#define MAX_ATTENDANCE    10000

#define DATA_FILE         "church_members.txt"
#define TEMP_FILE         "church_members.tmp"

#define LIST_END          0
#define LIST_BEGINNING    1

/* ------------------------------ DATA TYPES ----------------------------- */

/* A church member (used by the array, the linked list, and the stack). */
typedef struct {
    char id[ID_LEN];
    char firstName[NAME_LEN];
    char lastName[NAME_LEN];
    char gender[GENDER_LEN];
    char phone[PHONE_LEN];
    char email[EMAIL_LEN];
    char dateOfBirth[DATE_LEN];
    char membershipDate[DATE_LEN];
    char ministry[MINISTRY_LEN];
    char status[STATUS_LEN];
    int  attendance;                /* number of attendance sessions        */
} Member;

/* Node of the singly linked list (active members). */
typedef struct Node {
    Member data;
    struct Node *next;
} Node;

/* One church service request (queue item). */
typedef struct {
    int  requestId;
    char memberId[ID_LEN];
    char memberName[FULLNAME_LEN];
    char type[TYPE_LEN];
    char description[DESC_LEN];
    char date[DATE_LEN];
    char status[STATUS_LEN];
} ServiceRequest;

/* Circular queue (FIFO) with front, rear, count and storage. */
typedef struct {
    ServiceRequest items[QUEUE_CAPACITY];
    int front;
    int rear;
    int count;
} RequestQueue;

/* Kinds of operations that can be undone. */
typedef enum { OP_REGISTER, OP_UPDATE, OP_DELETE } OperationType;

/* One stack entry: what happened + the member data needed to reverse it. */
typedef struct {
    OperationType operation;
    Member snapshot;     /* REGISTER: new member | UPDATE: OLD state | DELETE: deleted member */
    int arrayIndex;      /* position in the array (used to restore a deleted member)          */
} HistoryEntry;

/* Stack (LIFO). */
typedef struct {
    HistoryEntry items[STACK_CAPACITY];
    int top;             /* -1 means empty */
} HistoryStack;

/* ---------------------------- GLOBAL VARIABLES ------------------------- */
static Member        members[MAX_MEMBERS];   /* Question 1: the array        */
static int           memberCount = 0;        /* number of members in array   */
static Node         *head = NULL;            /* Question 2: linked list head */
static RequestQueue  requestQueue;           /* Question 3: queue            */
static HistoryStack  history;                /* Question 5: stack            */
static int           nextRequestId = 1;
static char          sortLabel[64] = "Registration order (not sorted)";

/* ANSI colour codes (set to "" when the terminal does not support colours). */
static const char *CLR_RESET  = "";
static const char *CLR_RED    = "";
static const char *CLR_GREEN  = "";
static const char *CLR_YELLOW = "";
static const char *CLR_CYAN   = "";
static const char *CLR_BOLD   = "";

static const char *MINISTRIES[] = { "Choir", "Youth", "Ushering", "Deacon",
                                    "Prayer Team", "Children", "Women", "Men", "Other" };
#define MINISTRY_COUNT 9

static const char *REQUEST_TYPES[]  = { "Membership", "Baptism", "Counseling",
                                        "Prayer", "Transfer", "Activity" };
static const char *REQUEST_LABELS[] = { "Membership registration request", "Baptism request",
                                        "Counseling request", "Prayer request",
                                        "Transfer request", "Church activity participation request" };
#define REQUEST_TYPE_COUNT 6

/* ------------------------- FUNCTION PROTOTYPES ------------------------- */
static void freeLinkedList(void);
static void syncLinkedListForMember(const char *id);
static int  saveMembers(void);
static int  loadMembers(void);

/* ======================================================================== */
/*                         SECTION A: UI UTILITIES                          */
/* ======================================================================== */

/* Turn on ANSI colours when the terminal supports them. */
static void initColors(void)
{
    int enable = 0;
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        if (SetConsoleMode(h, mode | 0x0004)) {   /* ENABLE_VIRTUAL_TERMINAL_PROCESSING */
            enable = 1;
        }
    }
#else
    enable = isatty(STDOUT_FILENO);
#endif
    if (enable) {
        CLR_RESET  = "\033[0m";
        CLR_RED    = "\033[31m";
        CLR_GREEN  = "\033[32m";
        CLR_YELLOW = "\033[33m";
        CLR_CYAN   = "\033[36m";
        CLR_BOLD   = "\033[1m";
    }
}

static void printSuccess(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s", CLR_GREEN);
    vprintf(fmt, ap);
    printf("%s\n", CLR_RESET);
    va_end(ap);
}

static void printError(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s", CLR_RED);
    vprintf(fmt, ap);
    printf("%s\n", CLR_RESET);
    va_end(ap);
}

static void printWarning(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s", CLR_YELLOW);
    vprintf(fmt, ap);
    printf("%s\n", CLR_RESET);
    va_end(ap);
}

static void printInfo(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    printf("%s", CLR_CYAN);
    vprintf(fmt, ap);
    printf("%s\n", CLR_RESET);
    va_end(ap);
}

static void printLine(char ch, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        putchar(ch);
    }
    putchar('\n');
}

static void printBanner(const char *title)
{
    printf("\n%s", CLR_BOLD);
    printLine('=', 46);
    printf("   %s\n", title);
    printLine('=', 46);
    printf("%s", CLR_RESET);
}

/* Safe string copy: never overflows the destination. */
static void copyStr(char *dst, size_t size, const char *src)
{
    size_t n = strlen(src);
    if (n >= size) {
        n = size - 1;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}
#define COPY(dst, src) copyStr((dst), sizeof(dst), (src))

static void trimInPlace(char *s)
{
    size_t len = strlen(s);
    size_t start = 0;
    size_t end = len;
    while (s[start] != '\0' && isspace((unsigned char)s[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char)s[end - 1])) {
        end--;
    }
    memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

static void toUpperStr(char *s)
{
    for (; *s != '\0'; s++) {
        *s = (char)toupper((unsigned char)*s);
    }
}

/* Free memory and leave the program (also used when input ends unexpectedly). */
static void exitProgram(int code)
{
    freeLinkedList();
    exit(code);
}

/*
 readLine: reads one line safely with fgets (never overflows the buffer),
 removes the newline, trims spaces, and rejects lines that are too long.
*/
static void readLine(const char *prompt, char *buf, size_t size)
{
    for (;;) {
        size_t len;
        if (prompt != NULL) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if (fgets(buf, (int)size, stdin) == NULL) {
            printf("\nInput stream closed. Exiting program.\n");
            exitProgram(0);
        }
        len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        } else if (!feof(stdin)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF) {
                /* discard the rest of the long line */
            }
            printError("Error: Input too long (maximum %d characters).", (int)size - 1);
            continue;
        }
        trimInPlace(buf);
        return;
    }
}

/* Converts text to a whole number. Returns 1 if the WHOLE text is a number. */
static int parseWholeNumber(const char *text, long *value)
{
    char *end;
    if (text[0] == '\0') {
        return 0;
    }
    errno = 0;
    *value = strtol(text, &end, 10);
    if (*end != '\0' || errno == ERANGE) {
        return 0;
    }
    return 1;
}

/* Reads an integer between min and max (inclusive); repeats until valid. */
static int readInt(const char *prompt, int min, int max)
{
    char buf[LINE_LEN];
    long value;
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        if (!parseWholeNumber(buf, &value)) {
            printError("Error: Invalid input! Please enter a whole number.");
            continue;
        }
        if (value < 0 && min >= 0) {
            printError("Error: Negative values are not allowed!");
            continue;
        }
        if (value < min || value > max) {
            printError("Error: Number must be between %d and %d.", min, max);
            continue;
        }
        return (int)value;
    }
}

/* Reads a menu choice from 0 to maxChoice. Invalid selections are rejected. */
static int readMenuChoice(int maxChoice)
{
    char buf[LINE_LEN];
    long value;
    for (;;) {
        readLine("Enter your choice: ", buf, sizeof(buf));
        if (parseWholeNumber(buf, &value) && value >= 0 && value <= maxChoice) {
            return (int)value;
        }
        printError("Error: Invalid choice! Please enter a number from 0 to %d.", maxChoice);
    }
}

/* Asks a yes/no question. Returns 1 for yes and 0 for no. */
static int confirmYesNo(const char *prompt)
{
    char buf[LINE_LEN];
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        if (buf[0] == 'y' || buf[0] == 'Y') {
            if (buf[1] == '\0' || strcmp(buf, "yes") == 0 || strcmp(buf, "YES") == 0) return 1;
        }
        if (buf[0] == 'n' || buf[0] == 'N') {
            if (buf[1] == '\0' || strcmp(buf, "no") == 0 || strcmp(buf, "NO") == 0) return 0;
        }
        printError("Error: Please answer with y or n.");
    }
}

static void getTodayString(char *out)
{
    time_t t = time(NULL);
    struct tm *tmInfo = localtime(&t);
    strftime(out, DATE_LEN, "%Y-%m-%d", tmInfo);
}

/* ======================================================================== */
/*                       SECTION B: VALIDATION FUNCTIONS                    */
/* ======================================================================== */

static int isLeapYear(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

/* Checks the format YYYY-MM-DD and that the date really exists. */
static int isValidDateFormat(const char *s)
{
    int i, y, m, d;
    int daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (strlen(s) != 10) return 0;
    for (i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (s[i] != '-') return 0;
        } else if (!isdigit((unsigned char)s[i])) {
            return 0;
        }
    }
    if (sscanf(s, "%4d-%2d-%2d", &y, &m, &d) != 3) return 0;
    if (y < 1900 || m < 1 || m > 12 || d < 1) return 0;
    if (m == 2 && isLeapYear(y)) daysInMonth[1] = 29;
    return d <= daysInMonth[m - 1];
}

static int isValidId(const char *s)
{
    size_t i, len = strlen(s);
    if (len < 1 || len >= ID_LEN) return 0;
    for (i = 0; i < len; i++) {
        if (!isalnum((unsigned char)s[i]) && s[i] != '-') return 0;
    }
    return 1;
}

static int isValidName(const char *s)
{
    size_t i, len = strlen(s);
    int hasLetter = 0;
    if (len < 1 || len >= NAME_LEN) return 0;
    for (i = 0; i < len; i++) {
        if (isalpha((unsigned char)s[i])) hasLetter = 1;
        else if (s[i] != ' ' && s[i] != '-' && s[i] != '\'') return 0;
    }
    return hasLetter;
}

static int isValidPhone(const char *s)
{
    size_t i, len = strlen(s);
    size_t start = (s[0] == '+') ? 1 : 0;
    if (len - start < 9 || len - start > 15 || len >= PHONE_LEN) return 0;
    for (i = start; i < len; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

static int isValidEmail(const char *s)
{
    size_t i, len = strlen(s);
    const char *at;
    const char *dot;
    if (len < 5 || len >= EMAIL_LEN) return 0;
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (!isalnum(c) && c != '.' && c != '-' && c != '_' && c != '+' && c != '@') return 0;
    }
    at = strchr(s, '@');
    if (at == NULL || at == s || strchr(at + 1, '@') != NULL) return 0;
    dot = strrchr(at, '.');
    if (dot == NULL || dot == at + 1 || dot[1] == '\0') return 0;
    return 1;
}

static int isValidStatus(const char *s)
{
    return strcmp(s, "Active") == 0 || strcmp(s, "Inactive") == 0 || strcmp(s, "Transferred") == 0;
}

/* Full validation of one record (used when loading from the file). */
static int validateMemberRecord(const Member *m)
{
    if (!isValidId(m->id)) return 0;
    if (!isValidName(m->firstName) || !isValidName(m->lastName)) return 0;
    if (strcmp(m->gender, "Male") != 0 && strcmp(m->gender, "Female") != 0) return 0;
    if (!isValidPhone(m->phone) || !isValidEmail(m->email)) return 0;
    if (!isValidDateFormat(m->dateOfBirth) || !isValidDateFormat(m->membershipDate)) return 0;
    if (strcmp(m->membershipDate, m->dateOfBirth) < 0) return 0;
    if (m->ministry[0] == '\0') return 0;
    if (!isValidStatus(m->status)) return 0;
    if (m->attendance < 0 || m->attendance > MAX_ATTENDANCE) return 0;
    return 1;
}

/* ======================================================================== */
/*                          SECTION C: INPUT HELPERS                        */
/* ======================================================================== */

static void readMemberId(const char *prompt, char *out)
{
    char buf[LINE_LEN];
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        toUpperStr(buf);
        if (isValidId(buf)) {
            copyStr(out, ID_LEN, buf);
            return;
        }
        printError("Error: Invalid Member ID! Use 1-%d letters/digits (example: M001).", ID_LEN - 1);
    }
}

static void inputName(const char *prompt, char *out)
{
    char buf[LINE_LEN];
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        if (buf[0] == '\0') {
            printError("Error: This field cannot be empty!");
        } else if (!isValidName(buf)) {
            printError("Error: Use letters, spaces, '-' or ' only (maximum %d characters).", NAME_LEN - 1);
        } else {
            copyStr(out, NAME_LEN, buf);
            return;
        }
    }
}

static void inputGender(char *out)
{
    printf("  Gender: 1) Male  2) Female\n");
    copyStr(out, GENDER_LEN, readInt("  Select gender (1-2): ", 1, 2) == 1 ? "Male" : "Female");
}

static void inputPhone(char *out)
{
    char buf[LINE_LEN];
    for (;;) {
        readLine("Phone number (9-15 digits, e.g., 0780000000): ", buf, sizeof(buf));
        if (buf[0] == '\0') {
            printError("Error: Phone number cannot be empty!");
        } else if (!isValidPhone(buf)) {
            printError("Error: Invalid phone number! Use 9 to 15 digits (an optional leading '+').");
        } else {
            copyStr(out, PHONE_LEN, buf);
            return;
        }
    }
}

static void inputEmail(char *out)
{
    char buf[LINE_LEN];
    for (;;) {
        readLine("Email address: ", buf, sizeof(buf));
        if (buf[0] == '\0') {
            printError("Error: Email address cannot be empty!");
        } else if (!isValidEmail(buf)) {
            printError("Error: Invalid email address (example: name@church.org).");
        } else {
            copyStr(out, EMAIL_LEN, buf);
            return;
        }
    }
}

/* Reads a date (YYYY-MM-DD). minDate / maxDate may be NULL. A date can never be in the future. */
static void inputDate(const char *prompt, char *out, const char *minDate, const char *maxDate)
{
    char buf[LINE_LEN];
    char today[DATE_LEN];
    const char *limit;
    getTodayString(today);
    limit = (maxDate != NULL && strcmp(maxDate, today) < 0) ? maxDate : today;
    for (;;) {
        readLine(prompt, buf, sizeof(buf));
        if (buf[0] == '\0') {
            printError("Error: Date cannot be empty!");
        } else if (!isValidDateFormat(buf)) {
            printError("Error: Invalid date! Use the format YYYY-MM-DD (example: 2015-06-07).");
        } else if (minDate != NULL && strcmp(buf, minDate) < 0) {
            printError("Error: Date cannot be earlier than %s.", minDate);
        } else if (strcmp(buf, limit) > 0) {
            printError("Error: Date cannot be later than %s.", limit);
        } else {
            copyStr(out, DATE_LEN, buf);
            return;
        }
    }
}

static void inputMinistry(char *out)
{
    int i, choice;
    printf("  Ministry / Department:\n");
    for (i = 0; i < MINISTRY_COUNT; i++) {
        printf("    %d) %s\n", i + 1, MINISTRIES[i]);
    }
    choice = readInt("  Select ministry: ", 1, MINISTRY_COUNT);
    copyStr(out, MINISTRY_LEN, MINISTRIES[choice - 1]);
}

static void inputStatus(char *out)
{
    const char *options[] = { "Active", "Inactive", "Transferred" };
    int choice;
    printf("  Membership status: 1) Active  2) Inactive  3) Transferred\n");
    choice = readInt("  Select status (1-3): ", 1, 3);
    copyStr(out, STATUS_LEN, options[choice - 1]);
}

static void inputAttendance(int *out)
{
    *out = readInt("Number of attendance sessions (0-10000): ", 0, MAX_ATTENDANCE);
}

/* ======================================================================== */
/*                     SECTION D: DISPLAY HELPERS (TABLES)                  */
/* ======================================================================== */

static void getFullName(const Member *m, char *out, size_t size)
{
    snprintf(out, size, "%s %s", m->firstName, m->lastName);
}

static void printTableHeader(void)
{
    printf("%s", CLR_BOLD);
    printLine('-', 89);
    printf("%-3s %-8s %-22s %-14s %-12s %-12s %10s\n",
           "No.", "ID", "NAME", "PHONE", "MINISTRY", "STATUS", "ATTENDANCE");
    printLine('-', 89);
    printf("%s", CLR_RESET);
}

static void printMemberRow(int no, const Member *m)
{
    char fullName[FULLNAME_LEN];
    const char *color = CLR_RESET;
    getFullName(m, fullName, sizeof(fullName));
    if (strcmp(m->status, "Active") == 0) color = CLR_GREEN;
    else if (strcmp(m->status, "Inactive") == 0) color = CLR_RED;
    else color = CLR_YELLOW;
    printf("%-3d %-8s %-22.22s %-14s %-12.12s %s%-12s%s %10d\n",
           no, m->id, fullName, m->phone, m->ministry, color, m->status, CLR_RESET, m->attendance);
}

static void displayMemberTable(const Member *list, int n, const char *title)
{
    int i;
    printf("\n%s%s%s\n", CLR_BOLD, title, CLR_RESET);
    printTableHeader();
    for (i = 0; i < n; i++) {
        printMemberRow(i + 1, &list[i]);
    }
    printLine('-', 89);
}

static void printMemberDetails(const Member *m)
{
    char fullName[FULLNAME_LEN];
    getFullName(m, fullName, sizeof(fullName));
    printf("\n  ----------- MEMBER DETAILS -----------\n");
    printf("  Member ID        : %s\n", m->id);
    printf("  Name             : %s\n", fullName);
    printf("  Gender           : %s\n", m->gender);
    printf("  Phone            : %s\n", m->phone);
    printf("  Email            : %s\n", m->email);
    printf("  Date of birth    : %s\n", m->dateOfBirth);
    printf("  Membership date  : %s\n", m->membershipDate);
    printf("  Ministry         : %s\n", m->ministry);
    printf("  Status           : %s\n", m->status);
    printf("  Attendance       : %d session(s)\n", m->attendance);
    printf("  --------------------------------------\n");
}

/* ======================================================================== */
/*              SECTION E: QUESTION 1 - ARRAY (CORE FUNCTIONS)              */
/* ======================================================================== */

/* Returns the array index of a member ID, or -1 when it does not exist. */
static int findMemberIndex(const char *id)
{
    int i;
    for (i = 0; i < memberCount; i++) {
        if (strcmp(members[i].id, id) == 0) return i;
    }
    return -1;
}

/* Adds a member at the end of the array. Returns 0 when the array is full. */
static int addMemberToArray(const Member *m)
{
    if (memberCount >= MAX_MEMBERS) return 0;
    members[memberCount++] = *m;
    COPY(sortLabel, "Registration order (not sorted)");
    return 1;
}

/* Removes the member at "index" and SHIFTS the remaining elements left. */
static void removeMemberFromArray(int index)
{
    int i;
    for (i = index; i < memberCount - 1; i++) {
        members[i] = members[i + 1];
    }
    memberCount--;
    COPY(sortLabel, "Registration order (not sorted)");
}

/* Inserts a member at a given position (shifts elements right). Used by UNDO of delete. */
static int insertMemberAt(int index, const Member *m)
{
    int i;
    if (memberCount >= MAX_MEMBERS) return 0;
    if (index < 0 || index > memberCount) index = memberCount;
    for (i = memberCount; i > index; i--) {
        members[i] = members[i - 1];
    }
    members[index] = *m;
    memberCount++;
    COPY(sortLabel, "Registration order (not sorted)");
    return 1;
}

/* ======================================================================== */
/*          SECTION F: QUESTION 2 - SINGLY LINKED LIST (CORE FUNCTIONS)     */
/* ======================================================================== */

/* Creates a node. Every malloc() is checked for failure. */
static Node *createNode(const Member *m)
{
    Node *node = (Node *)malloc(sizeof(Node));
    if (node == NULL) {
        printError("Error: Memory allocation failed!");
        return NULL;
    }
    node->data = *m;
    node->next = NULL;
    return node;
}

/* Insert a member at the BEGINNING of the list. */
static int insertMemberBeginning(const Member *m)
{
    Node *node = createNode(m);
    if (node == NULL) return 0;
    node->next = head;
    head = node;
    return 1;
}

/* Insert a member at the END of the list. */
static int insertMemberEnd(const Member *m)
{
    Node *node = createNode(m);
    Node *cur;
    if (node == NULL) return 0;
    if (head == NULL) {            /* empty list: the new node becomes the head */
        head = node;
        return 1;
    }
    cur = head;
    while (cur->next != NULL) {    /* walk to the last node */
        cur = cur->next;
    }
    cur->next = node;
    return 1;
}

/* Searches the list. Returns the node (or NULL) and its 1-based position. */
static Node *searchLinkedMember(const char *id, int *position)
{
    Node *cur = head;
    int pos = 1;
    while (cur != NULL) {
        if (strcmp(cur->data.id, id) == 0) {
            if (position != NULL) *position = pos;
            return cur;
        }
        cur = cur->next;
        pos++;
    }
    if (position != NULL) *position = 0;
    return NULL;
}

/* Deletes a node by Member ID. Handles first, middle and last nodes. Returns 1 if deleted. */
static int deleteLinkedMember(const char *id)
{
    Node *cur = head;
    Node *prev = NULL;
    while (cur != NULL && strcmp(cur->data.id, id) != 0) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) return 0;          /* not found                     */
    if (prev == NULL) {
        head = cur->next;               /* deleting the FIRST node       */
    } else {
        prev->next = cur->next;         /* deleting a MIDDLE/LAST node   */
    }
    free(cur);
    return 1;
}

static int countLinkedMembers(void)
{
    int count = 0;
    Node *cur = head;
    while (cur != NULL) {
        count++;
        cur = cur->next;
    }
    return count;
}

/* Releases every node (called before the program ends). */
static void freeLinkedList(void)
{
    Node *cur = head;
    while (cur != NULL) {
        Node *next = cur->next;
        free(cur);
        cur = next;
    }
    head = NULL;
}

/*
 syncLinkedListForMember: the heart of the integration between array and list.
   - The member exists in the array AND is Active  -> make sure the list has an
     up-to-date copy (update it, or add it at the end).
   - Otherwise (deleted / Inactive / Transferred)  -> make sure it is NOT in the list.
*/
static void syncLinkedListForMember(const char *id)
{
    int idx = findMemberIndex(id);
    Node *node = searchLinkedMember(id, NULL);
    if (idx >= 0 && strcmp(members[idx].status, "Active") == 0) {
        if (node != NULL) node->data = members[idx];
        else insertMemberEnd(&members[idx]);
    } else if (node != NULL) {
        deleteLinkedMember(id);
    }
}

/* Rebuilds the entire list from the array (used after loading a file). */
static void rebuildLinkedList(void)
{
    int i;
    freeLinkedList();
    for (i = 0; i < memberCount; i++) {
        if (strcmp(members[i].status, "Active") == 0) {
            insertMemberEnd(&members[i]);
        }
    }
}

/* ======================================================================== */
/*                SECTION G: QUESTION 5 - STACK (CORE FUNCTIONS)            */
/* ======================================================================== */

static int isStackEmpty(void) { return history.top < 0; }
static int isStackFull(void)  { return history.top >= STACK_CAPACITY - 1; }

/* PUSH: saves one modification. On overflow the OLDEST entry is discarded (with a warning). */
static int pushHistory(OperationType op, const Member *snapshot, int index)
{
    if (isStackFull()) {
        printWarning("Warning: History stack overflow - oldest history entry was discarded.");
        memmove(&history.items[0], &history.items[1], sizeof(HistoryEntry) * (STACK_CAPACITY - 1));
        history.top--;
    }
    history.top++;
    history.items[history.top].operation = op;
    history.items[history.top].snapshot = *snapshot;
    history.items[history.top].arrayIndex = index;
    return 1;
}

/* POP: removes and returns the most recent entry. Returns 0 on underflow. */
static int popHistory(HistoryEntry *out)
{
    if (isStackEmpty()) return 0;
    *out = history.items[history.top];
    history.top--;
    return 1;
}

static const char *operationName(OperationType op)
{
    switch (op) {
        case OP_REGISTER: return "REGISTER";
        case OP_UPDATE:   return "UPDATE";
        default:          return "DELETE";
    }
}

/* Displays the history from the TOP (newest) of the stack to the BOTTOM (oldest). */
static void displayHistory(void)
{
    int i, no = 1;
    if (isStackEmpty()) {
        printWarning("No modification history available!");
        return;
    }
    printf("\n%sMODIFICATION HISTORY (top = most recent)%s\n", CLR_BOLD, CLR_RESET);
    printLine('-', 78);
    printf("%-4s %-10s %-8s %-22s %s\n", "No.", "OPERATION", "ID", "NAME", "UNDO WILL...");
    printLine('-', 78);
    for (i = history.top; i >= 0; i--) {
        const HistoryEntry *e = &history.items[i];
        char fullName[FULLNAME_LEN];
        const char *action;
        getFullName(&e->snapshot, fullName, sizeof(fullName));
        if (e->operation == OP_REGISTER) action = "remove the registered member";
        else if (e->operation == OP_UPDATE) action = "restore the old member data";
        else action = "bring the deleted member back";
        printf("%-4d %-10s %-8s %-22.22s %s%s\n", no++, operationName(e->operation),
               e->snapshot.id, fullName, action, (i == history.top) ? "   <-- TOP" : "");
    }
    printLine('-', 78);
    printf("Entries in stack: %d / %d\n", history.top + 1, STACK_CAPACITY);
}

/* UNDO: pops the latest operation and reverses it. */
static void undoLastOperation(void)
{
    HistoryEntry e;
    int idx;
    if (!popHistory(&e)) {
        printWarning("Nothing to undo!");
        return;
    }
    idx = findMemberIndex(e.snapshot.id);
    switch (e.operation) {
        case OP_REGISTER:                       /* reverse a registration = delete it */
            if (idx < 0) {
                printError("Error: Cannot undo - member %s no longer exists.", e.snapshot.id);
                return;
            }
            removeMemberFromArray(idx);
            syncLinkedListForMember(e.snapshot.id);
            printInfo("Undid REGISTER: member %s was removed.", e.snapshot.id);
            break;
        case OP_UPDATE:                         /* reverse an update = restore the old data */
            if (idx < 0) {
                printError("Error: Cannot undo - member %s no longer exists.", e.snapshot.id);
                return;
            }
            members[idx] = e.snapshot;
            syncLinkedListForMember(e.snapshot.id);
            printInfo("Undid UPDATE: old data of member %s was restored.", e.snapshot.id);
            break;
        default:                                /* reverse a deletion = put the member back */
            if (idx >= 0) {
                printError("Error: Cannot undo - Member ID %s already exists again.", e.snapshot.id);
                pushHistory(e.operation, &e.snapshot, e.arrayIndex);
                return;
            }
            if (!insertMemberAt(e.arrayIndex, &e.snapshot)) {
                printError("Error: Cannot undo - the member array is full!");
                pushHistory(e.operation, &e.snapshot, e.arrayIndex);
                return;
            }
            syncLinkedListForMember(e.snapshot.id);
            printInfo("Undid DELETE: member %s was restored.", e.snapshot.id);
            break;
    }
    printSuccess("Changes restored successfully!");
}

/* ======================================================================== */
/*              SECTION H: QUESTION 1 - ARRAY (MENU FUNCTIONS)              */
/* ======================================================================== */

/*
 insertMember (Register New Member): reads and validates all fields, rejects duplicates,
 stores the member in the array, saves history on the stack, and adds Active members to the
 linked list (at the end by default, or at the beginning when called from the list menu).
*/
static void insertMember(const char *presetId, int listPosition)
{
    Member m;
    int index;
    memset(&m, 0, sizeof(m));

    if (memberCount >= MAX_MEMBERS) {
        printError("Error: Member array is full (%d/%d)! Cannot register more members.",
                   memberCount, MAX_MEMBERS);
        return;
    }
    if (presetId != NULL) {
        COPY(m.id, presetId);
        printf("Member ID: %s\n", m.id);
    } else {
        readMemberId("Member ID (example: M001): ", m.id);
    }
    if (findMemberIndex(m.id) >= 0) {
        printError("Error: Member ID already exists!");
        return;
    }
    inputName("First name: ", m.firstName);
    inputName("Last name: ", m.lastName);
    inputGender(m.gender);
    inputPhone(m.phone);
    inputEmail(m.email);
    inputDate("Date of birth (YYYY-MM-DD): ", m.dateOfBirth, NULL, NULL);
    inputDate("Membership date (YYYY-MM-DD): ", m.membershipDate, m.dateOfBirth, NULL);
    inputMinistry(m.ministry);
    inputStatus(m.status);
    inputAttendance(&m.attendance);

    index = memberCount;
    addMemberToArray(&m);
    pushHistory(OP_REGISTER, &m, index);
    printSuccess("Member registered successfully!");

    if (strcmp(m.status, "Active") == 0) {
        int ok = (listPosition == LIST_BEGINNING) ? insertMemberBeginning(&m) : insertMemberEnd(&m);
        if (ok) printInfo("Member added to the active linked list.");
    } else {
        printInfo("Member is %s, so not added to the active linked list.", m.status);
    }
}

/* displayMembers: shows every member of the array in a table. */
static void displayMembers(void)
{
    if (memberCount == 0) {
        printWarning("No members registered yet!");
        return;
    }
    displayMemberTable(members, memberCount, "ALL REGISTERED MEMBERS (ARRAY)");
    printf("Total members: %d / %d\n", memberCount, MAX_MEMBERS);
}

/* searchMember: finds a member by ID in the array. */
static void searchMember(void)
{
    char id[ID_LEN];
    int idx;
    if (memberCount == 0) {
        printWarning("No members registered yet! Nothing to search.");
        return;
    }
    readMemberId("Enter Member ID to search: ", id);
    idx = findMemberIndex(id);
    if (idx < 0) {
        printError("Error: Member not found!");
        return;
    }
    printSuccess("Member found at array position %d.", idx + 1);
    printMemberDetails(&members[idx]);
}

/* Edits ONE field of a working copy of a member. */
static void editField(Member *w, int field)
{
    switch (field) {
        case 1: inputName("New first name: ", w->firstName); break;
        case 2: inputName("New last name: ", w->lastName); break;
        case 3: inputGender(w->gender); break;
        case 4: inputPhone(w->phone); break;
        case 5: inputEmail(w->email); break;
        case 6: inputDate("New date of birth (YYYY-MM-DD): ", w->dateOfBirth, NULL, w->membershipDate); break;
        case 7: inputDate("New membership date (YYYY-MM-DD): ", w->membershipDate, w->dateOfBirth, NULL); break;
        case 8: inputMinistry(w->ministry); break;
        case 9: inputStatus(w->status); break;
        default: inputAttendance(&w->attendance); break;
    }
}

/*
 updateMember: the old state is saved on the stack BEFORE the change, then the linked list is
 synchronised (e.g., a member changed to Inactive leaves the active list).
*/
static void updateMember(void)
{
    char id[ID_LEN];
    Member original, working;
    int idx, field, changed = 0;

    if (memberCount == 0) {
        printWarning("No members registered yet! Nothing to update.");
        return;
    }
    readMemberId("Enter Member ID to update: ", id);
    idx = findMemberIndex(id);
    if (idx < 0) {
        printError("Error: Member not found!");
        return;
    }
    original = members[idx];
    working = original;
    printInfo("Note: the Member ID cannot be changed.");
    for (;;) {
        printMemberDetails(&working);
        printf("  Which field do you want to change?\n");
        printf("   1) First name      2) Last name        3) Gender\n");
        printf("   4) Phone           5) Email            6) Date of birth\n");
        printf("   7) Membership date 8) Ministry         9) Status\n");
        printf("  10) Attendance      0) Finish / save changes\n");
        field = readInt("  Select field (0-10): ", 0, 10);
        if (field == 0) break;
        editField(&working, field);
        changed = 1;
    }
    if (!changed) {
        printInfo("No changes were made.");
        return;
    }
    members[idx] = working;
    pushHistory(OP_UPDATE, &original, idx);
    syncLinkedListForMember(id);
    printSuccess("Member updated successfully!");
}

/* deleteMember: removes a member (after confirmation), shifting the array elements left. */
static void deleteMember(void)
{
    char id[ID_LEN];
    int idx;
    if (memberCount == 0) {
        printWarning("No members registered yet! Nothing to delete.");
        return;
    }
    readMemberId("Enter Member ID to delete: ", id);
    idx = findMemberIndex(id);
    if (idx < 0) {
        printError("Error: Member not found!");
        return;
    }
    printMemberDetails(&members[idx]);
    if (!confirmYesNo("Are you sure you want to delete this member? (y/n): ")) {
        printInfo("Deletion cancelled.");
        return;
    }
    pushHistory(OP_DELETE, &members[idx], idx);   /* save BEFORE deleting */
    removeMemberFromArray(idx);
    syncLinkedListForMember(id);                  /* removes it from the list as well */
    printSuccess("Member deleted successfully!");
}

/* ======================================================================== */
/*              SECTION I: QUESTION 2 - LINKED LIST (MENU FUNCTIONS)        */
/* ======================================================================== */

static void displayLinkedMembers(void)
{
    Node *cur = head;
    int no = 1;
    if (head == NULL) {
        printWarning("The linked list is empty! No active members.");
        return;
    }
    printf("\n%sACTIVE MEMBERS (SINGLY LINKED LIST)%s\n", CLR_BOLD, CLR_RESET);
    printTableHeader();
    while (cur != NULL) {
        printMemberRow(no++, &cur->data);
        cur = cur->next;
    }
    printLine('-', 89);
    printf("Chain: HEAD");
    for (cur = head; cur != NULL; cur = cur->next) {
        printf(" -> %s", cur->data.id);
    }
    printf(" -> NULL\n");
}

/* Insert at beginning/end of the list. The member must exist in the array (single source of truth). */
static void insertLinkedMenu(int listPosition)
{
    char id[ID_LEN];
    int idx;
    readMemberId("Enter Member ID to insert into the list: ", id);
    idx = findMemberIndex(id);
    if (idx < 0) {
        printError("Error: Member ID is not in the main array!");
        if (confirmYesNo("Register this ID as a new member now? (y/n): ")) {
            insertMember(id, listPosition);
        }
        return;
    }
    if (searchLinkedMember(id, NULL) != NULL) {
        printError("Error: Member ID already exists in the linked list!");
        return;
    }
    if (strcmp(members[idx].status, "Active") != 0) {
        Member old = members[idx];
        printWarning("Member is %s. Only Active members can be in this list, so the status", old.status);
        printWarning("will be changed to Active (this change can be undone).");
        COPY(members[idx].status, "Active");
        pushHistory(OP_UPDATE, &old, idx);
    }
    if ((listPosition == LIST_BEGINNING) ? insertMemberBeginning(&members[idx])
                                         : insertMemberEnd(&members[idx])) {
        printSuccess("Member inserted at the %s of the linked list!",
                     listPosition == LIST_BEGINNING ? "beginning" : "end");
    }
}

static void searchLinkedMenu(void)
{
    char id[ID_LEN];
    int pos;
    Node *node;
    if (head == NULL) {
        printWarning("The linked list is empty!");
        return;
    }
    readMemberId("Enter Member ID to search in the list: ", id);
    node = searchLinkedMember(id, &pos);
    if (node == NULL) {
        printError("Error: Member not found in the linked list!");
        return;
    }
    printSuccess("Member found at node position %d.", pos);
    printMemberDetails(&node->data);
}

/*
 Delete from the active list. A member that leaves the ACTIVE list is no longer active, so the
 record stays in the array with status "Inactive" (church records are never lost by accident).
*/
static void deleteLinkedMenu(void)
{
    char id[ID_LEN];
    int idx;
    if (head == NULL) {
        printWarning("The linked list is empty! Nothing to delete.");
        return;
    }
    readMemberId("Enter Member ID to delete from the list: ", id);
    if (searchLinkedMember(id, NULL) == NULL) {
        printError("Error: Member not found in the linked list!");
        return;
    }
    if (!confirmYesNo("Remove this member from the active list? (y/n): ")) {
        printInfo("Deletion cancelled.");
        return;
    }
    idx = findMemberIndex(id);
    if (idx >= 0) {
        Member old = members[idx];
        COPY(members[idx].status, "Inactive");
        pushHistory(OP_UPDATE, &old, idx);
    }
    deleteLinkedMember(id);
    printSuccess("Member deleted from the linked list successfully!");
    printInfo("The array record was kept and its status is now Inactive.");
}

static void countLinkedMenu(void)
{
    printInfo("Total nodes in the linked list: %d", countLinkedMembers());
}

/* ======================================================================== */
/*                SECTION J: QUESTION 3 - QUEUE (FIFO)                      */
/* ======================================================================== */

static void initQueue(void)
{
    requestQueue.front = 0;
    requestQueue.rear = -1;
    requestQueue.count = 0;
}

static int isQueueEmpty(void) { return requestQueue.count == 0; }
static int isQueueFull(void)  { return requestQueue.count == QUEUE_CAPACITY; }

/* ENQUEUE: adds a request at the REAR. Returns 0 if the queue is full. */
static int enqueueRequest(const ServiceRequest *req)
{
    if (isQueueFull()) return 0;
    requestQueue.rear = (requestQueue.rear + 1) % QUEUE_CAPACITY;
    requestQueue.items[requestQueue.rear] = *req;
    requestQueue.count++;
    return 1;
}

/* DEQUEUE: removes the request at the FRONT (the oldest). Returns 0 if the queue is empty. */
static int dequeueRequest(ServiceRequest *out)
{
    if (isQueueEmpty()) return 0;
    *out = requestQueue.items[requestQueue.front];
    requestQueue.front = (requestQueue.front + 1) % QUEUE_CAPACITY;
    requestQueue.count--;
    return 1;
}

/* displayRequests: pending requests from FRONT (oldest) to REAR (newest). */
static void displayRequests(void)
{
    int k;
    if (isQueueEmpty()) {
        printWarning("Queue is empty!");
        return;
    }
    printf("\n%sPENDING SERVICE REQUESTS (FIFO order)%s\n", CLR_BOLD, CLR_RESET);
    printLine('-', 96);
    printf("%-4s %-6s %-8s %-18s %-12s %-11s %-26s\n",
           "POS", "REQ", "MEMBER", "NAME", "TYPE", "DATE", "DESCRIPTION");
    printLine('-', 96);
    for (k = 0; k < requestQueue.count; k++) {
        const ServiceRequest *r = &requestQueue.items[(requestQueue.front + k) % QUEUE_CAPACITY];
        printf("%-4d R%-5.3d %-8s %-18.18s %-12s %-11s %-26.26s%s\n",
               k + 1, r->requestId, r->memberId, r->memberName, r->type, r->date, r->description,
               k == 0 ? "  <-- FRONT" : (k == requestQueue.count - 1 ? "  <-- REAR" : ""));
    }
    printLine('-', 96);
    printf("Pending requests: %d / %d\n", requestQueue.count, QUEUE_CAPACITY);
}

/* Builds a request for a member and adds it to the queue. */
static int addRequestFor(const Member *m, const char *type, const char *description)
{
    ServiceRequest r;
    memset(&r, 0, sizeof(r));
    r.requestId = nextRequestId;
    COPY(r.memberId, m->id);
    getFullName(m, r.memberName, sizeof(r.memberName));
    COPY(r.type, type);
    COPY(r.description, description);
    getTodayString(r.date);
    COPY(r.status, "Pending");
    if (!enqueueRequest(&r)) return 0;
    nextRequestId++;
    return 1;
}

static void submitRequest(void)
{
    char id[ID_LEN];
    char desc[DESC_LEN];
    int idx, i, choice;
    if (isQueueFull()) {
        printError("Queue is full! (%d/%d requests pending) Process a request first.",
                   requestQueue.count, QUEUE_CAPACITY);
        return;
    }
    if (memberCount == 0) {
        printWarning("No members registered yet! Register a member first.");
        return;
    }
    readMemberId("Member ID submitting the request: ", id);
    idx = findMemberIndex(id);
    if (idx < 0) {
        printError("Error: Member not found!");
        return;
    }
    printInfo("Member: %s %s", members[idx].firstName, members[idx].lastName);
    printf("  Request types:\n");
    for (i = 0; i < REQUEST_TYPE_COUNT; i++) {
        printf("    %d) %s\n", i + 1, REQUEST_LABELS[i]);
    }
    choice = readInt("  Select request type (1-6): ", 1, REQUEST_TYPE_COUNT);
    for (;;) {
        readLine("Request description: ", desc, sizeof(desc));
        if (desc[0] != '\0') break;
        printError("Error: Description cannot be empty!");
    }
    if (addRequestFor(&members[idx], REQUEST_TYPES[choice - 1], desc)) {
        printSuccess("Request R%03d submitted successfully! Position in queue: %d",
                     nextRequestId - 1, requestQueue.count);
    }
}

/* Processes the OLDEST request (FIFO). A processed Transfer request marks the member Transferred. */
static void processOldestRequest(void)
{
    ServiceRequest r;
    if (!dequeueRequest(&r)) {
        printWarning("Queue is empty!");
        return;
    }
    COPY(r.status, "Processed");
    printSuccess("Processing the oldest request (FIFO)...");
    printf("  Request ID  : R%03d\n  Member      : %s (%s)\n  Type        : %s\n"
           "  Description : %s\n  Date        : %s\n  Status      : %s\n",
           r.requestId, r.memberName, r.memberId, r.type, r.description, r.date, r.status);
    if (strcmp(r.type, "Transfer") == 0) {
        int idx = findMemberIndex(r.memberId);
        if (idx >= 0 && strcmp(members[idx].status, "Transferred") != 0) {
            Member old = members[idx];
            COPY(members[idx].status, "Transferred");
            pushHistory(OP_UPDATE, &old, idx);
            syncLinkedListForMember(r.memberId);
            printInfo("Member %s is now marked Transferred and left the active list.", r.memberId);
        }
    }
    printInfo("Remaining pending requests: %d", requestQueue.count);
}

/* ======================================================================== */
/*         SECTION K: QUESTION 4 - SEARCHING AND SORTING ALGORITHMS         */
/* ======================================================================== */

/* Linear Search: checks the array one element at a time from position 1. */
static void linearSearchMember(void)
{
    char id[ID_LEN];
    int i, found = -1;
    if (memberCount == 0) {
        printWarning("No members registered yet! The array is empty.");
        return;
    }
    readMemberId("Enter Member ID for Linear Search: ", id);
    for (i = 0; i < memberCount; i++) {
        if (strcmp(members[i].id, id) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        printError("Error: Member not found! (%d comparisons made)", memberCount);
        return;
    }
    printSuccess("Member found after %d comparison(s)!", found + 1);
    printInfo("Array position: %d (index %d)", found + 1, found);
    printMemberDetails(&members[found]);
}

/*
 Binary Search: needs data sorted by Member ID. The array is copied and the COPY is sorted by ID
 (insertion sort), so the main array order (for example the attendance sort) is not disturbed.
 Note: IDs are compared as text, like a dictionary (M001 < M002 < M010).
*/
static void binarySearchMember(void)
{
    Member sorted[MAX_MEMBERS];
    char id[ID_LEN];
    int i, j, low, high, mid, step = 0, found = -1;
    if (memberCount == 0) {
        printWarning("No members registered yet! The array is empty.");
        return;
    }
    readMemberId("Enter Member ID for Binary Search: ", id);

    memcpy(sorted, members, sizeof(Member) * (size_t)memberCount);
    for (i = 1; i < memberCount; i++) {                /* insertion sort by ID (ascending) */
        Member key = sorted[i];
        j = i - 1;
        while (j >= 0 && strcmp(sorted[j].id, key.id) > 0) {
            sorted[j + 1] = sorted[j];
            j--;
        }
        sorted[j + 1] = key;
    }
    printInfo("Records were sorted by Member ID (ascending) before searching.");

    low = 0;
    high = memberCount - 1;
    while (low <= high) {
        int cmp;
        mid = low + (high - low) / 2;
        cmp = strcmp(id, sorted[mid].id);
        step++;
        printf("  Step %d: low=%d high=%d mid=%d -> ID at middle = %s : ",
               step, low + 1, high + 1, mid + 1, sorted[mid].id);
        if (cmp == 0) {
            printf("MATCH\n");
            found = mid;
            break;
        } else if (cmp < 0) {
            printf("target is smaller, search the left half\n");
            high = mid - 1;
        } else {
            printf("target is larger, search the right half\n");
            low = mid + 1;
        }
    }
    if (found < 0) {
        printError("Error: Member not found! (%d step(s) used)", step);
        return;
    }
    printSuccess("Member found in %d step(s)! Position in ID-sorted order: %d", step, found + 1);
    printInfo("Position in the main array: %d", findMemberIndex(id) + 1);
    printMemberDetails(&sorted[found]);
}

/* Bubble Sort by attendance. Whole Member records are swapped (not only the numbers). */
static void bubbleSortMembers(int ascending)
{
    int i, j, comparisons = 0, swaps = 0;
    for (i = 0; i < memberCount - 1; i++) {
        int swapped = 0;
        for (j = 0; j < memberCount - 1 - i; j++) {
            int outOfOrder;
            comparisons++;
            if (ascending) outOfOrder = members[j].attendance > members[j + 1].attendance;
            else           outOfOrder = members[j].attendance < members[j + 1].attendance;
            if (outOfOrder) {
                Member temp = members[j];
                members[j] = members[j + 1];
                members[j + 1] = temp;
                swaps++;
                swapped = 1;
            }
        }
        if (!swapped) break;       /* already sorted: stop early */
    }
    COPY(sortLabel, ascending ? "Attendance - ASCENDING (lowest first)"
                              : "Attendance - DESCENDING (highest first)");
    printInfo("Bubble Sort finished: %d comparisons, %d swaps.", comparisons, swaps);
}

static void sortMenu(int ascending)
{
    if (memberCount == 0) {
        printWarning("No members registered yet! Nothing to sort.");
        return;
    }
    displayMemberTable(members, memberCount, "ORIGINAL RECORDS (before sorting)");
    printInfo("Selected sorting order: %s", ascending ? "ASCENDING by attendance"
                                                       : "DESCENDING by attendance");
    bubbleSortMembers(ascending);
    displayMemberTable(members, memberCount, "SORTED RECORDS (after Bubble Sort)");
}

static void displaySortedMembers(void)
{
    if (memberCount == 0) {
        printWarning("No members registered yet!");
        return;
    }
    printInfo("Current order: %s", sortLabel);
    displayMemberTable(members, memberCount, "MEMBERS IN CURRENT ARRAY ORDER");
}

/* ======================================================================== */
/*                      SECTION L: FILE HANDLING                            */
/* ======================================================================== */
/*
 File format (text, one member per line, fields separated by '|'):
   id|firstName|lastName|gender|phone|email|dateOfBirth|membershipDate|ministry|status|attendance
 Lines that start with '#' are comments. The file is written to a temporary file first and then
 renamed, so a crash while saving can never destroy the previous good file.
*/

static int saveMembers(void)
{
    FILE *fp = fopen(TEMP_FILE, "w");
    int i;
    if (fp == NULL) {
        printError("Error: Cannot create file '%s' (%s).", TEMP_FILE, strerror(errno));
        return 0;
    }
    fprintf(fp, "# Church Membership Management System - data file\n");
    fprintf(fp, "# id|first|last|gender|phone|email|dob|membershipDate|ministry|status|attendance\n");
    for (i = 0; i < memberCount; i++) {
        const Member *m = &members[i];
        fprintf(fp, "%s|%s|%s|%s|%s|%s|%s|%s|%s|%s|%d\n", m->id, m->firstName, m->lastName,
                m->gender, m->phone, m->email, m->dateOfBirth, m->membershipDate,
                m->ministry, m->status, m->attendance);
    }
    if (ferror(fp) || fclose(fp) != 0) {
        printError("Error: Writing to the file failed (%s).", strerror(errno));
        return 0;
    }
    remove(DATA_FILE);
    if (rename(TEMP_FILE, DATA_FILE) != 0) {
        printError("Error: Cannot replace '%s' (%s). Data is kept in '%s'.", DATA_FILE,
                   strerror(errno), TEMP_FILE);
        return 0;
    }
    printSuccess("%d member record(s) saved to '%s' successfully!", memberCount, DATA_FILE);
    return 1;
}

/* Splits "a|b|c" in place. Returns the number of fields found. */
static int splitFields(char *line, char *fields[], int maxFields)
{
    int n = 0;
    fields[n++] = line;
    while (*line != '\0' && n < maxFields) {
        if (*line == '|') {
            *line = '\0';
            fields[n++] = line + 1;
        }
        line++;
    }
    return n;
}

/* Loads the file into the array. Returns -1 if the file is missing, else the number loaded. */
static int loadMembers(void)
{
    FILE *fp = fopen(DATA_FILE, "r");
    char line[512];
    int loaded = 0, skipped = 0;
    if (fp == NULL) {
        return -1;
    }
    memberCount = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *f[12];
        Member m;
        char *end;
        long att;
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') continue;
        if (splitFields(line, f, 12) != 11) { skipped++; continue; }
        memset(&m, 0, sizeof(m));
        COPY(m.id, f[0]);          toUpperStr(m.id);
        COPY(m.firstName, f[1]);   COPY(m.lastName, f[2]);
        COPY(m.gender, f[3]);      COPY(m.phone, f[4]);
        COPY(m.email, f[5]);       COPY(m.dateOfBirth, f[6]);
        COPY(m.membershipDate, f[7]); COPY(m.ministry, f[8]);
        COPY(m.status, f[9]);
        att = strtol(f[10], &end, 10);
        if (*end != '\0' || f[10][0] == '\0') { skipped++; continue; }
        m.attendance = (int)att;
        if (!validateMemberRecord(&m) || findMemberIndex(m.id) >= 0 || memberCount >= MAX_MEMBERS) {
            skipped++;
            continue;
        }
        members[memberCount++] = m;
        loaded++;
    }
    fclose(fp);
    COPY(sortLabel, "Registration order (not sorted)");
    rebuildLinkedList();          /* keep the list consistent with the array */
    history.top = -1;             /* old history no longer matches the loaded data */
    if (skipped > 0) {
        printWarning("Warning: %d invalid/duplicate line(s) in the file were skipped.", skipped);
    }
    return loaded;
}

/* Sample data: 10 members + 3 requests (for testing and for the demonstration). */
static const Member SAMPLE_MEMBERS[] = {
    {"M001", "John",   "Smith",     "Male",   "0780000000", "john.smith@church.org",    "1985-03-12", "2015-06-07", "Choir",       "Active",      25},
    {"M002", "Mary",   "Johnson",   "Female", "0781111111", "mary.johnson@church.org",  "1990-07-21", "2016-02-14", "Youth",       "Active",      18},
    {"M003", "Peter",  "James",     "Male",   "0782222222", "peter.james@church.org",   "1978-11-02", "2012-09-23", "Ushering",    "Inactive",    12},
    {"M004", "Grace",  "Uwase",     "Female", "0783333333", "grace.uwase@church.org",   "1995-01-30", "2018-04-15", "Choir",       "Active",      31},
    {"M005", "David",  "Mugisha",   "Male",   "0784444444", "david.mugisha@church.org", "1982-05-18", "2010-01-10", "Deacon",      "Active",      40},
    {"M006", "Esther", "Ingabire",  "Female", "0785555555", "esther.ingabire@church.org","1988-09-09","2014-08-03", "Prayer Team", "Active",      22},
    {"M007", "Samuel", "Niyonzima", "Male",   "0786666666", "samuel.niyonzima@church.org","2001-12-25","2020-03-01","Youth",       "Active",       9},
    {"M008", "Ruth",   "Mukamana",  "Female", "0787777777", "ruth.mukamana@church.org", "1975-04-04", "2005-05-22", "Women",       "Transferred", 15},
    {"M009", "Joseph", "Habimana",  "Male",   "0788888888", "joseph.habimana@church.org","1992-06-16","2017-10-29", "Ushering",    "Active",      27},
    {"M010", "Sarah",  "Uwimana",   "Female", "0789999999", "sarah.uwimana@church.org", "1999-08-08", "2019-07-14", "Children",    "Active",      14}
};
#define SAMPLE_MEMBER_COUNT ((int)(sizeof(SAMPLE_MEMBERS) / sizeof(SAMPLE_MEMBERS[0])))

static void loadSampleData(void)
{
    int i, added = 0, skipped = 0, reqAdded = 0;
    const char *reqMember[] = { "M002", "M004", "M007" };
    const char *reqType[]   = { "Counseling", "Prayer", "Baptism" };
    const char *reqDesc[]   = { "Family counseling session", "Prayer for exam success", "Wants to be baptised in December" };

    for (i = 0; i < SAMPLE_MEMBER_COUNT; i++) {
        if (findMemberIndex(SAMPLE_MEMBERS[i].id) >= 0) { skipped++; continue; }
        if (!addMemberToArray(&SAMPLE_MEMBERS[i])) {
            printError("Error: Member array is full!");
            break;
        }
        syncLinkedListForMember(SAMPLE_MEMBERS[i].id);
        added++;
    }
    for (i = 0; i < 3; i++) {
        int idx = findMemberIndex(reqMember[i]);
        if (idx >= 0 && addRequestFor(&members[idx], reqType[i], reqDesc[i])) reqAdded++;
    }
    printSuccess("Sample data: %d member(s) added, %d skipped (already exist), %d request(s) queued.",
                 added, skipped, reqAdded);
}

/* ======================================================================== */
/*                        SECTION M: MENUS AND MAIN                         */
/* ======================================================================== */

static void memberMenu(void)
{
    int choice;
    do {
        printBanner("1. MEMBER RECORD MANAGEMENT (ARRAY)");
        printf(" 1. Register New Member\n 2. View All Members\n 3. Search Member\n"
               " 4. Update Member\n 5. Delete Member\n 0. Return to Main Menu\n");
        choice = readMenuChoice(5);
        switch (choice) {
            case 1: insertMember(NULL, LIST_END); break;
            case 2: displayMembers(); break;
            case 3: searchMember(); break;
            case 4: updateMember(); break;
            case 5: deleteMember(); break;
            default: break;
        }
    } while (choice != 0);
}

static void linkedListMenu(void)
{
    int choice;
    do {
        printBanner("2. DYNAMIC MEMBERSHIP MANAGEMENT (LINKED LIST)");
        printf(" 1. Insert at Beginning\n 2. Insert at End\n 3. Search Member\n"
               " 4. Delete Member\n 5. Display Members\n 6. Count Members\n 0. Return to Main Menu\n");
        choice = readMenuChoice(6);
        switch (choice) {
            case 1: insertLinkedMenu(LIST_BEGINNING); break;
            case 2: insertLinkedMenu(LIST_END); break;
            case 3: searchLinkedMenu(); break;
            case 4: deleteLinkedMenu(); break;
            case 5: displayLinkedMembers(); break;
            case 6: countLinkedMenu(); break;
            default: break;
        }
    } while (choice != 0);
}

static void requestMenu(void)
{
    int choice;
    do {
        printBanner("3. CHURCH SERVICE REQUEST MANAGEMENT (QUEUE)");
        printf(" 1. Submit New Request\n 2. Process Oldest Request\n"
               " 3. View Pending Requests\n 0. Return to Main Menu\n");
        choice = readMenuChoice(3);
        switch (choice) {
            case 1: submitRequest(); break;
            case 2: processOldestRequest(); break;
            case 3: displayRequests(); break;
            default: break;
        }
    } while (choice != 0);
}

static void searchSortMenu(void)
{
    int choice;
    do {
        printBanner("4. MEMBER SEARCHING AND SORTING");
        printf(" 1. Linear Search\n 2. Binary Search\n 3. Sort by Attendance (Ascending)\n"
               " 4. Sort by Attendance (Descending)\n 5. Display Sorted Members\n 0. Return to Main Menu\n");
        choice = readMenuChoice(5);
        switch (choice) {
            case 1: linearSearchMember(); break;
            case 2: binarySearchMember(); break;
            case 3: sortMenu(1); break;
            case 4: sortMenu(0); break;
            case 5: displaySortedMembers(); break;
            default: break;
        }
    } while (choice != 0);
}

static void historyMenu(void)
{
    int choice;
    do {
        printBanner("5. MODIFICATION HISTORY AND UNDO (STACK)");
        printf(" 1. View Modification History\n 2. Undo Last Operation\n 0. Return to Main Menu\n");
        choice = readMenuChoice(2);
        switch (choice) {
            case 1: displayHistory(); break;
            case 2: undoLastOperation(); break;
            default: break;
        }
    } while (choice != 0);
}

static void fileMenu(void)
{
    int choice, result;
    do {
        printBanner("7. SAVE AND LOAD MEMBER RECORDS");
        printf(" 1. Save Records to File\n 2. Load Records from File\n"
               " 3. Load Sample Data (10 members + 3 requests)\n 0. Return to Main Menu\n");
        choice = readMenuChoice(3);
        switch (choice) {
            case 1: saveMembers(); break;
            case 2:
                if (memberCount > 0 &&
                    !confirmYesNo("Loading REPLACES the current records and clears history. Continue? (y/n): ")) {
                    printInfo("Load cancelled.");
                    break;
                }
                result = loadMembers();
                if (result < 0) printError("Error: File '%s' not found or cannot be opened!", DATA_FILE);
                else printSuccess("%d member record(s) loaded successfully!", result);
                break;
            case 3: loadSampleData(); break;
            default: break;
        }
    } while (choice != 0);
}

static void printMainMenu(void)
{
    printf("\n%s", CLR_BOLD);
    printLine('=', 46);
    printf("       CHURCH MEMBERSHIP MANAGEMENT SYSTEM\n");
    printLine('=', 46);
    printf("%s", CLR_RESET);
    printf(" 1. MEMBER RECORD MANAGEMENT (ARRAY)\n");
    printf(" 2. DYNAMIC MEMBERSHIP MANAGEMENT (LINKED LIST)\n");
    printf(" 3. CHURCH SERVICE REQUEST MANAGEMENT (QUEUE)\n");
    printf(" 4. MEMBER SEARCHING AND SORTING\n");
    printf(" 5. MODIFICATION HISTORY AND UNDO (STACK)\n");
    printf(" 6. DISPLAY ALL MEMBERS\n");
    printf(" 7. SAVE AND LOAD MEMBER RECORDS\n");
    printf(" 0. EXIT SYSTEM\n");
    printLine('=', 46);
}

int main(void)
{
    int choice, loaded;

    initColors();
    initQueue();
    history.top = -1;

    loaded = loadMembers();
    if (loaded < 0) {
        printInfo("No saved data file ('%s') found. Starting with empty records.", DATA_FILE);
    } else {
        printSuccess("%d member record(s) loaded from '%s'.", loaded, DATA_FILE);
    }

    do {
        printMainMenu();
        choice = readMenuChoice(7);
        switch (choice) {
            case 1: memberMenu(); break;
            case 2: linkedListMenu(); break;
            case 3: requestMenu(); break;
            case 4: searchSortMenu(); break;
            case 5: historyMenu(); break;
            case 6: displayMembers(); break;
            case 7: fileMenu(); break;
            default: break;
        }
    } while (choice != 0);

    if (confirmYesNo("Save member records before exiting? (y/n): ")) {
        saveMembers();
    }
    freeLinkedList();      /* release all dynamically allocated nodes */
    printf("\nThank you for using the Church Membership Management System. God bless you!\n");
    return 0;
}