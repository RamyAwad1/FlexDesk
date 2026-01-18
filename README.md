Concurrent Co-Working Space DBMS

A custom-built, in-memory database management system written in C. This project simulates the backend architecture of a co-working space management platform, designed to handle concurrent user operations efficiently.

🚀 Technical Highlights 

Concurrency Control: Implemented Read-Write Locks (pthread_rwlock) to optimize throughput. Multiple threads can read data simultaneously (e.g., displaying members), while write operations (e.g., adding bookings) obtain exclusive locks to prevent race conditions.

O(1) Indexing: Engineered a Hash Map Index with chaining for the Member table, reducing lookup time from $O(N)$ (Linear Search) to $O(1)$ (Constant Time).

Thread-Safe Logging: Built a custom audit logging system using Mutexes to serialize write operations to system.log.

Data Integrity: Enforces strict foreign key constraints (e.g., a Booking cannot be created for a non-existent Member or Workspace) and unique constraints (Email uniqueness).

🛠 Features

Members Management: Add, Update, Delete, and rapid Lookup via Hash Index.

Workspace Management: Manage inventory of desks and rooms.

Bookings & Payments: Transactional linking between members, workspaces, and financial records.

Persistence: State is persisted to CSV files (members.csv, workspaces.csv, etc.) upon exit.

Concurrency Demo: Built-in stress test mode to visually demonstrate locking mechanics (Readers overlapping vs. Writers blocking).

📦 Building and Running

Prerequisites

GCC Compiler

Pthread library (standard on most Linux/Unix/macOS systems)

Compilation

Use the -pthread flag to link the threading library:

gcc -o dbms Final_db_prog.c -pthread


Execution

./dbms


🧪 Concurrency Demo

To see the locking mechanism in action:

Run the program.

Select option 88 (RUN CONCURRENCY TEST).

Observe the console output:

Readers will acquire locks simultaneously.

Writers will wait until all active readers are finished before acquiring an exclusive lock.

🔮 Future Roadmap

Paging & Buffer Pool: Implementing LRU caching to handle data sets larger than RAM.

Binary Storage: Migrating from CSV to custom binary file formats for improved I/O performance.

Client-Server Architecture: Decoupling the CLI to allow remote socket connections.

📄 License

Open Source.