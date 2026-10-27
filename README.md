# mail-c
An E-mail application implementation to satisfy a course of "Network Programming" in BIT Fall 2026 semester.

## TECH STACK
- C
- SQLite
- UNIX socket API

## TODO
- [x] config file parsing (key value pair in .txt)
- [x] UNIX socket programming 
- [x] implement CRLF for receiving TCP messages
- [x] generic thread pool
- [x] simple CLI (enter username, password, send email)
- [x] implement the barebone SMTP protocol
    - [x] client send command
    - [x] server send reply
    - [x] client parse reply
    - [x] server parse command
- [x] DB connection
- [x] log system
- [x] integrate accounts
- [x] POP3 protocol
    - [x] USER, PASS
    - [x] STAT, LIST
    - [x] RETR
    - [x] QUIT
- [x] IMF construction
- [ ] gui

## HIGHLIGHTS
- minimum dependencies (base lib)
- sane defaults (arena allocator, length-based string, per-thread log)
- no malloc
- lightning fast build time (unity build, no linking, 675ms debug build, 4.5s -O3 build)
- concurrent client handling (with threadpool)

## CHALLENGES
- unfamiliarity with the new defaults (especially the log system)
- bad API design for the protocols

## LIMITATION5
- Server didn't check for IMF compatibility
- no authentication for SMTP listener (only enforced by client-side)
- SQLite is assumed to work properly (minimal error handling)
- no interface for registration (only through direct manipulation of db with query or viewer)
- most responses are sent in bulk, ignoring the protocols maximum and recommended length limit
- POP3 doesn't have state checks
- logging is mixed with control flow
- no relay network handling (emails to unknown users are ignored)
- no password encryption (got in the log too!)
