# Church Membership Management System

## Project Information

- System: Church Membership Management System
- Course: INSY 8321 - Data Structures and Algorithms
- Institution: Adventist University of Central Africa
- Student Name: MUGISHA Espoir
- Student ID: 27824
- Project No.: 31

## Overview

This console application manages church member records and service requests. It demonstrates an array, singly linked list, circular queue, stack, searching, sorting, and file storage.

## Main Idea

The project provides a simple digital register for a church. Instead of keeping member information and requests in separate paper records or scattered notebooks, the system stores organized information in one program. A church administrator can register members, update their details, record attendance, identify active members, accept service requests, search for members, sort records, undo changes, and save information for future use.

The system is intended as an educational implementation of data structures and algorithms. It shows how different structures solve different problems:

- An array provides direct access to every member record.
- A linked list represents active members that can change in status.
- A queue processes service requests in the order in which they arrive.
- A stack keeps a history that allows administrators to undo operations.
- Search and sorting functions help an administrator find and organize information quickly.
- File storage allows records to continue existing after the program closes.

## Problem Addressed

Churches often manage information manually. Paper records can be misplaced, become difficult to update, take time to search, or contain conflicting information. For example, an administrator may need to check whether a person is a member, find a phone number, update a ministry assignment, respond to a baptism request, or process a transfer. Manual searching and writing can make these tasks slow and can increase the chance of errors.

This project provides a clear and consistent computerized process for those tasks. It gives information a predictable structure and separates member records, active membership, service requests, and modification history.

## How the System Works

### 1. Program Startup

When the program starts, it initializes the in-memory queue and undo stack, enables console colors when supported, and attempts to load `church_members.txt`. If the file is not present, the system starts with an empty member array and displays an informational message.

For every saved member that has an active status, the program rebuilds the linked list so that the linked list and array begin in a consistent state.

### 2. Member Registration and Storage

The administrator enters member details such as:

- Member identification number
- First and last name
- Gender
- Phone number
- Email address
- Date of birth
- Membership date
- Ministry
- Status
- Attendance count

The member is added to the primary array. A successful registration is also saved as a stack history entry so that it can be undone.

### 3. Active Members Linked List

The linked list contains only members whose status is `Active`. It is not a second source of permanent records. The array remains the primary storage.

The system rebuilds or updates the linked list whenever member status or records change. This makes the list useful for displaying active members and demonstrates how linked nodes can be created, searched, inserted, displayed, or removed at runtime.

### 4. Service Request Queue

Church members can submit requests for services such as:

- Membership
- Baptism
- Counseling
- Prayer
- Activity participation
- Transfer

The queue stores request records in first-in, first-out order. Before a request is submitted, the system verifies that the member ID belongs to an existing member. The administrator can view the next request, process it, or leave it waiting.

When a transfer request is processed, the member's status is changed to `Transferred` and that member is removed from the active-member linked list.

### 5. Search and Sorting

The system searches the member array according to the selected criteria. It can help administrators find member records rather than manually reviewing the complete list.

Sorting arranges member records according to a chosen field and updates a sort label so the administrator can know how the current records are organized. Search and sort operations work on the array, which keeps member storage consistent.

### 6. Modification History and Undo

Every registration, update, or deletion can be recorded in the stack. The latest operation is the first operation that undo reverses.

- Registration undo removes the newly registered member.
- Update undo restores the member's previous values.
- Deletion undo restores the deleted member at its former array position.

After undo, the system rebuilds or synchronizes the active linked list.

### 7. Display and Reporting

The system can display all stored members or the subset represented by the active-member linked list. This allows administrators to view the organization of the data and verify that changes are reflected consistently.

### 8. Save and Exit

The program opens `church_members.txt` when loading or saving records. Saving writes the member array to temporary storage and then replaces the saved file. This helps prevent the previous file from being directly overwritten while the save operation is in progress.

Before exit, the program asks whether the administrator wants to save the member records. Saved data is loaded automatically the next time the program starts.

## Data Structures

1. **Array (`members[]`)**: Primary member storage. Every member record is stored in this array.
2. **Linked list (`head`)**: Contains active members only. It is synchronized with the array after member changes.
3. **Circular queue (`requestQueue`)**: Stores service requests in first-in, first-out order. Requests require an existing member. Processing a transfer request changes the member status to Transferred and removes the member from the active linked list.
4. **Search and sort**: Operates directly on the primary member array.
5. **Stack (`history`)**: Records registration, update, and deletion operations. Undo restores the relevant array entry and rebuilds the active linked list.

Only the member array is saved to disk. At startup, records are loaded from `church_members.txt` and the active-member linked list is rebuilt.

## Main Menu

The application provides the following sections:

1. **Member record management**: Register, update, delete, or search individual members.
2. **Dynamic membership management**: Work with active-member linked-list operations.
3. **Church service request management**: Submit and process church requests.
4. **Searching and sorting**: Find and arrange member records.
5. **Modification history and undo**: Reverse recent register, update, and delete operations.
6. **Display all members**: View member records currently stored in the array.
7. **Save and load records**: Store or retrieve member information from disk.
0. **Exit**: Finish the program and offer to save changes.

## Step-by-Step Verification

Use this walkthrough to check that the main features work and to see what output to expect. It creates and changes test records, so perform it in a separate copy of the project rather than in a folder containing important member data.

### Prepare a Safe Test Copy

1. In File Explorer, copy the project folder to a separate test folder.
2. If the copied folder contains `church_members.txt`, rename that file in the test copy (for example, to `church_members.backup.txt`). This lets the test start with a clean set of records while preserving the original file.
3. In the test copy, double-click `run.bat`. It compiles the program and opens its console window. Keep this window open while testing.
4. The main menu should display options `1` through `7` and `0` for exit. If compilation fails, the launcher prints an error instead of starting the menu.

In the examples below, enter the number for the requested menu option and press **Enter**. In a submenu, enter `0` to return to the main menu.

### Load Demonstration Data

1. From the main menu, enter `7` to open **Save and Load Member Records**.
2. Enter `3` for **Load Sample Data**.
3. Expect a success message reporting **10 members added** and **3 requests queued**.
4. Enter `0` to return to the main menu.

The sample members have IDs `M001` through `M010`. The three requests are for `M002` (Counseling), `M004` (Prayer), and `M007` (Baptism). Loading sample data adds it to the current session; do not select it repeatedly, or it will add more requests.

### Verify the Member Array and Search

1. Enter `6` from the main menu. The table should show 10 members and a total of `10 / 100`.
2. Enter `1` to open **Member Record Management**, then `3` to search.
3. Enter `M001`. The program should find and display John Smith.
4. Try an unknown ID such as `NO999`. The program should report that the member was not found.
5. Enter `0` to return to the main menu.

### Verify Registration, Update, Delete, and Undo

Register a test member:

1. From the main menu, enter `1`, then `1` to register.
2. At each prompt, enter these values:

   | Prompt | Test value |
   | --- | --- |
   | Member ID | `T001` |
   | First name | `Test` |
   | Last name | `Member` |
   | Gender | `1` (Male) |
   | Phone | `0781234567` |
   | Email | `test.member@example.com` |
   | Date of birth | `2000-01-01` |
   | Membership date | `2020-01-01` |
   | Ministry | `1` (Choir) |
   | Status | `1` (Active) |
   | Attendance | `7` |

3. Expect a registration success message and a message that the member was added to the active linked list.
4. From the member menu, enter `2` to view all members. There should now be 11 records, including `T001`.
5. Enter `4` to update a member, enter ID `T001`, choose field `10` (Attendance), and enter `8`. Choose field `0` to finish. Expect an update success message.
6. Return to the main menu with `0`. Enter `5`, then `1` to view history; the update should be listed.
7. Enter `2` in the history menu to undo. Leave the history menu with `0`, then enter `1` → `3` from the main menu and search for `T001`; attendance should be restored to `7`.
8. In the member menu, enter `5` to delete. Enter `T001`, confirm with `y`, then verify the record is gone using **View All Members**.
9. Leave the member menu with `0`, then enter `5` → `2` from the main menu to undo the deletion. Leave the history menu with `0`, then search for `T001` using `1` → `3`; it should be restored.

### Verify the Active-Member Linked List

1. From the main menu, enter `2` to open **Dynamic Membership Management**.
2. Enter `6` to count members. There should be 9 active members: the 8 active sample members plus `T001`.
3. Enter `3` and search for `T001`; it should be found in the list.
4. Enter `4`, enter `T001`, and confirm with `y` to remove it from the active list. The array record should remain, with status `Inactive`.
5. Leave the linked-list menu with `0`, then enter `5` → `2` from the main menu to undo. Leave the history menu with `0`, return to the linked-list menu, and count again; the count should return to 9.

### Verify Searching and Sorting

1. From the main menu, enter `4` → `1`, then search for `M001`. The linear search should find John Smith.
2. Enter `2` and search for `M010`. The binary search should report that it sorted a copy of the records by ID before searching and should find Sarah Uwimana.
3. Enter `3` to sort by attendance in ascending order, then `5` to display sorted members. `T001` (attendance 7) should be first and `M005` (attendance 40) last.
4. Enter `4` to sort in descending order, then `5` to display. `M005` should be first and `T001` last.

### Verify the Service-Request Queue

1. From the main menu, enter `3` → `3`. The three sample requests should appear in FIFO order: M002 Counseling, M004 Prayer, and M007 Baptism.
2. Enter `2` to process the oldest request. It should process M002's Counseling request and report two pending requests.
3. Enter `3` again. M004's Prayer request should now be at the front.
4. To test a transfer, leave the request menu with `0`, then enter `3` → `1`. Enter member ID `M001`, request type `5` (Transfer), and a description such as `Transfer test`.
5. View pending requests. The new transfer should appear after the older requests. Process requests with option `2` until the transfer is reached. The queue should process requests in arrival order and report that M001 was marked `Transferred`.
6. Leave the request menu with `0` and enter `6` to verify M001's status is `Transferred`. Enter `2` → `6` to verify the active-list count is now 8.
7. Leave the linked-list menu with `0`, then use main menu `5` → `2` to undo the transfer status change. Enter `6` again to verify M001 is Active; the active-list count should return to 9.

### Verify Saving and Loading

1. From the main menu, enter `7` → `1` to save. Expect a success message and a `church_members.txt` file in the test folder.
2. Leave the file menu with `0`, then exit with main menu option `0` and answer `y` to the save prompt.
3. Run `run.bat` again from the test folder. The startup message should report the saved member count, and menu `6` should show `T001`.
4. The request queue is in memory only, so pending requests do not persist after restarting. Member records do persist.
5. To test the load menu while records are already present, choose `7` → `2` and confirm with `y`. The saved member records should reload and the active linked list should be rebuilt.

### Expected Test Results

- Sample data: 10 members and 3 pending requests.
- After registering `T001`: 11 total records and 9 active members.
- Undoing an update restores the previous field value.
- Undoing a deletion restores the member record.
- Removing a member from the active list changes the status to Inactive but keeps the array record; undo restores Active status.
- Search finds known IDs and reports unknown IDs as not found.
- Attendance sorting puts the lowest or highest attendance first, depending on the selected order.
- Requests are processed oldest first.
- Saving and restarting preserves member records, but not pending requests or undo history.

## Importance

This project is important because it:

- Demonstrates how multiple data structures can cooperate in a complete application.
- Teaches practical programming concepts such as arrays, pointers, structures, dynamic memory, file input/output, and algorithm design.
- Shows the difference between primary storage and derived views of data.
- Provides a simple, understandable model for information management.
- Encourages orderly data entry and consistent record updates.
- Offers a foundation that can later be developed into a graphical or database-based system.
- Protects recent changes with an undo mechanism.
- Saves records for reuse instead of losing all work when the program closes.

## Social Benefits

A membership system can support the church and the wider community in several ways:

- **Better pastoral and administrative support:** Church leaders can identify members, ministries, and service needs more accurately.
- **Faster member assistance:** Staff can find contact information and submit or process requests more quickly.
- **Clear service organization:** Baptism, counseling, prayer, transfer, and activity requests can be handled in arrival order.
- **Improved communication:** Organized phone numbers and email addresses can help members receive announcements and pastoral messages.
- **Ministry coordination:** The system can help organize members into Choir, Youth, Ushering, Deacon, Prayer Team, Children, Women, Men, or Other groups.
- **Attendance follow-up:** Attendance totals can help leaders recognize engagement and identify members who may need encouragement or support.
- **Transparency and accountability:** Saved records and undo history make changes easier to understand and review.
- **Community care:** Reliable records can help leaders respond to vulnerable people, organize prayer or counseling support, and plan community activities.

The system should be used responsibly. Member information may contain personal and sensitive data. Access should be limited to authorized church representatives, records should be updated accurately, and data should be backed up and shared only for legitimate ministry purposes.

## Limitations

The current version is a console application and stores records in a plain text file. It does not provide user accounts, passwords, network access, database transactions, or role-based permissions. The in-memory queue is reset between program runs because only member records are saved to disk.

These limits are acceptable for a small educational prototype, but a real church deployment would benefit from database storage, authentication, privacy controls, regular backups, data validation, and a graphical interface.

## Requirements

- Windows
- GCC for Windows, such as WinLibs

## Run the System

Open PowerShell in this folder and run:

```powershell
.\run.bat
```

The launcher locates GCC, compiles `church membership system.c`, and starts `church_membership.exe`.

To compile and run manually:

```powershell
gcc "church membership system.c" -o "church_membership.exe"
.\church_membership.exe
```

## Data Files

- `church_members.txt`: Saved member records
- `church_members.tmp`: Temporary file used while saving
