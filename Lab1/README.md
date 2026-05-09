# The Journey of Reading and Writing a File

> Based on the C program that opens `sample.txt` for reading and appending, reads existing content,
> displays it on the terminal, appends a new line, and closes the file.

---

## The Code at a Glance

```c
int fd = open("sample.txt", O_RDWR | O_CREAT | O_APPEND, 0644);  // Step 1: Open
read(fd, buffer, sizeof(buffer));                                   // Step 2: Read existing content
write(1, buffer, bytesRead);                                        // Step 3: Display on terminal
write(fd, newData, 39);                                             // Step 4: Append new line to file
close(fd);                                                          // Step 5: Close
```

Five operations. Each one is simple from your program's perspective, but the OS does significant
work underneath to make each one happen safely and efficiently.

---

## The File : sample.txt

Before the program runs, `sample.txt` already contains:

```
Operating systems use file systems to organize and manage data stored on secondary storage
devices such as SSDs and hard disks. Every file is represented internally using metadata
structures called inodes, which store information like permissions, file size, timestamps,
and pointers to data blocks. When a process requests to read a file, the request is performed
through a system call that transitions execution from user mode to kernel mode. The kernel
then interacts with the Virtual File System (VFS), filesystem drivers, page cache, and disk
drivers to retrieve the required data efficiently. Modern operating systems also use mechanisms
such as DMA and memory mapping to optimize file access and reduce CPU overhead during data
transfer operations.
```

After the program runs, this line is appended at the end:

```
New line written using system calls.
```

---

## STEP 1 : `open("sample.txt", O_RDWR | O_CREAT | O_APPEND, 0644)`

### What your program does

You ask the OS: "Open `sample.txt`. I want to read from it and write to it. Create it if it does
not already exist. All writes must go to the end of the file. And if you do create it, give it
permissions `0644`."

That is a lot of instructions packed into one call. Here is what each flag means:

| Flag | Meaning |
|---|---|
| `O_RDWR` | Open for both reading and writing |
| `O_CREAT` | Create the file if it does not exist |
| `O_APPEND` | Before every write, move to the end of the file automatically |
| `0644` | File permissions: owner can read+write, others can only read |

### What the OS does

**The system call trap**

Your program lives in **User Mode**, it cannot touch disk hardware or kernel memory directly.
Calling `open()` triggers a **system call**, which causes the CPU to switch from:

```
User Mode  ->  Kernel Mode
```

The kernel now takes over.

**VFS : the universal layer**

The kernel does not talk to the disk directly. It first passes through the
**VFS (Virtual File System)**, a layer that provides a uniform interface for all filesystems
(ext4, NTFS, FAT32, etc.). This means your `open()` call works the same way regardless of what
filesystem the disk uses.

```
open()
  |
  v
VFS (universal layer)
  |
  v
ext4 (actual filesystem on disk)
```

**Finding the file using the inode**

The filesystem looks up `"sample.txt"` in the current directory. Here is the important detail:
**the filename is not stored with the file's data**. Instead:

- The **directory entry** maps `"sample.txt"` to an inode number
- The **inode** stores all the metadata: permissions, file size, timestamps, and pointers to the
  actual disk blocks that hold the content

```
"sample.txt"  ->  (directory lookup)  ->  inode 4821
inode 4821    ->  block 120, block 121, block 122, ...
```

Since `sample.txt` already exists, the OS uses the existing inode. If it did not exist,
`O_CREAT` would cause the OS to allocate a fresh inode and mark the file as empty.

**The O_APPEND effect**

The kernel notes in the file's internal state that this file descriptor has the append flag set.
Every time `write()` is called using this descriptor, the kernel will automatically move the
write position to the end of the file before writing, even if another process has added content
to the file in between. This makes appending atomic and safe.

**What `open()` returns**

The kernel creates a **file descriptor**, a small integer (say, `3`) that your program uses as
a handle for all future operations on this file.

```
fd = 3   (your program's ticket to access sample.txt)
```

If anything goes wrong (permission denied, disk full), `fd` is `-1` and your program prints the
error and exits.

---

## STEP 2 - `read(fd, buffer, sizeof(buffer))`

### What your program does

You say: "Read up to 128 bytes from the file into my buffer, starting from where the file
position currently is."

Because the file was opened with `O_APPEND`, the internal write cursor is at the end - but the
**read cursor starts at position 0**, the beginning of the file. So `read()` reads from the start.

### What the OS does - the full pipeline

**Another system call**

`read()` is also a system call. The CPU switches back to kernel mode.

**Check the Page Cache first**

The kernel maintains a **Page Cache** in RAM - a cache of recently accessed file data. Before
touching the disk, the kernel checks if the blocks for `sample.txt` are already cached.

```
Is the file data already in the Page Cache?
  YES  ->  copy it straight to your buffer (very fast, no disk access)
   NO  ->  go to the disk
```

For this example, assume the file has not been read recently, so the kernel must go to disk.

**Disk driver and DMA**

The kernel tells the disk driver: "Fetch blocks 120, 121, 122 from disk." The disk driver uses
**DMA (Direct Memory Access)** to carry out the transfer:

```
Disk Controller  --(DMA)-->  RAM (Page Cache)
```

DMA means the disk controller copies the data directly into RAM **without the CPU moving each
byte manually**. The CPU only sets up the transfer and receives an interrupt notification when it
is done. This keeps the CPU free to do other work during the transfer.

**Two copies - the normal read path**

Once the data is in the Page Cache (kernel memory), the kernel copies it into your program's
buffer:

```
Disk  --(DMA)-->  Page Cache (kernel RAM)  --(CPU copy)-->  buffer[128] (your program)
```

A normal `read()` always involves these two copies. This is a known cost of the standard I/O path.

The file's content - the paragraph about operating systems, inodes, and DMA - is now sitting in
`buffer`. Because the file is larger than 128 bytes, only the first 128 bytes are read this time.

**The file position advances**

After the read, the kernel advances the file's read cursor by `bytesRead`. A subsequent `read()`
call would continue from byte 128 onwards.

**Back to User Mode**

The kernel returns `bytesRead` (the number of bytes actually read) and hands control back to
your program.

---

## STEP 3 - `write(1, msg, 23)` and `write(1, buffer, bytesRead)`

### What your program does

First you print the label `"Existing File Content:\n"` to the terminal, then you print the
content of `buffer` - the first 128 bytes read from `sample.txt`.

Both writes go to **file descriptor 1**, which is always `stdout` (the terminal screen).

| fd | Always means |
|----|---|
| 0 | stdin - keyboard input |
| 1 | stdout - terminal screen |
| 2 | stderr - error output |

### What the OS does

Each `write(1, ...)` is a system call. The kernel:

1. Takes the data from your buffer
2. Passes it to the terminal driver
3. The terminal displays the characters on screen

No disk access happens here. This is purely a memory-to-screen operation.

The terminal output looks like this:

```
Existing File Content:
Operating systems use file systems to organize and manage data stored on secondary storage
devices such as SSDs and hard disks. Every file is represented internally using metadata
structures called inodes, which store information like permissions, file size, timestamps,
and pointers to data blocks. When a process requests to read a file...
```

---

## STEP 4 - `write(fd, newData, 39)`

### What your program does

You say: "Write 39 bytes of `newData` into the file using `fd`."

`newData` is:

```
\nNew line written using system calls.\n
```

### What the OS does

**Another system call - kernel mode again**

**The O_APPEND guarantee**

Because `fd` was opened with `O_APPEND`, the kernel automatically seeks to the **end of the file**
before writing, even if the file cursor is somewhere else due to the earlier `read()`. This happens
atomically - no other process can insert content between the seek and the write.

**Data goes to the Page Cache first**

Your 39 bytes do not go straight to disk. They go into the **Page Cache**, and that page is marked
as **dirty** - meaning it has been modified in RAM but not yet saved to disk.

```
newData (your buffer)  --(CPU copy)-->  Page Cache (marked dirty)
```

**Delayed write to disk - write-back caching**

The OS does not write to disk immediately. It waits and batches multiple writes together before
flushing, which is much more efficient than writing every small change right away.

The actual disk write happens when:
- The OS decides enough dirty pages have accumulated
- `close()` or `fsync()` is called
- The system is idle and decides to flush

When it does flush:

```
Page Cache (dirty pages)  --(DMA)-->  Disk
```

DMA again handles the physical transfer, keeping the CPU mostly free.

After this step, `sample.txt` on disk will end with:

```
...reduce CPU overhead during data transfer operations.
New line written using system calls.
```

---

## STEP 5 - `close(fd)`

### What your program does

You say: "I am done with this file. Release the file descriptor."

### What the OS does

The kernel:

- Removes `fd = 3` from your process's file descriptor table
- Frees the internal tracking structure (called an open file description) for this file
- Dirty pages associated with this file will eventually be flushed to disk by the OS
  (though not necessarily immediately at `close()` time - use `fsync()` before `close()` if you
  need a hard guarantee that data is on disk before the program exits)

The **Page Cache data stays in RAM**. The OS keeps it so that if anything reads `sample.txt`
again soon, it is served from memory without touching disk.

---

## Complete Journey - Visual Summary

```
Your Program
    |
    |  open("sample.txt", O_RDWR | O_CREAT | O_APPEND, 0644)
    v
+-------------------------------+
|  KERNEL (Kernel Mode)         |
|                               |
|  VFS -> ext4 filesystem       |
|          |                    |
|      Inode lookup             |
|      (locate disk blocks)     |
|      O_APPEND flag stored     |
+-------------------------------+
    |
    |  read(fd, buffer, 128)
    v
+-------------------------------+
|  Page Cache (Kernel RAM)      | <--(DMA)-- Disk (if not cached)
|  blocks of sample.txt         |
+-------------------------------+
    |
    |  CPU copy
    v
+-------------------------------+
|  buffer[128]                  |
|  (your program's memory)      |
+-------------------------------+
    |
    |  write(1, buffer, bytesRead)
    v
+-------------------------------+
|  Terminal / Screen            |
+-------------------------------+
    |
    |  write(fd, newData, 39)
    v
+-------------------------------+
|  Page Cache (marked dirty)    | --(DMA, later)--> Disk
|  new line appended at end     |
+-------------------------------+
    |
    |  close(fd)
    v
  Done. fd released. Dirty pages flushed by OS.
```

---

## Why O_APPEND Matters - A Concrete Example

Without `O_APPEND`, if two programs have the same file open and both try to write at the end,
they could overwrite each other:

```
Program A reads: file ends at byte 500
Program B reads: file ends at byte 500
Program A writes to byte 500  ->  okay
Program B writes to byte 500  ->  overwrites Program A's data (wrong)
```

With `O_APPEND`, the kernel handles the seek-then-write as a single atomic operation:

```
Program A: kernel seeks to end, writes  ->  file now ends at byte 539
Program B: kernel seeks to end, writes  ->  file now ends at byte 578  (correct, no overlap)
```

This is why `O_APPEND` is the right choice whenever multiple writers could touch the same file.

---

## Key Concepts Summary

| Concept | Simple Explanation |
|---|---|
| **System Call** | How your program asks the OS for help - CPU switches to kernel mode |
| **VFS** | A universal layer so the same code works on ext4, NTFS, or any filesystem |
| **Inode** | A file's internal record - stores metadata and pointers to disk blocks |
| **O_CREAT** | Tells the OS to create the file if it does not already exist |
| **O_APPEND** | Every write automatically goes to the end of the file, atomically |
| **0644** | Permissions: owner can read+write; group and others can only read |
| **Page Cache** | OS stores file data in RAM to avoid repeated disk reads |
| **DMA** | Hardware mechanism that moves data between disk and RAM without CPU involvement |
| **Dirty page** | A page in the Page Cache that has been written to but not yet saved to disk |
| **Write-back caching** | OS delays disk writes and batches them for efficiency |
| **fd = 1** | Always means stdout - the terminal screen |

---

## What Happens if the File Does Not Exist

Because `O_CREAT` is set, if `sample.txt` does not exist:

1. The kernel allocates a new inode
2. Sets permissions to `0644` (adjusted by the process's umask)
3. Creates a directory entry mapping `"sample.txt"` to the new inode
4. Returns a valid `fd`

The subsequent `read()` immediately returns `0` - nothing to read from an empty file. The
`write(fd, newData, 39)` then writes the file's first line. After `close()`, the file exists on
disk containing only that one line.