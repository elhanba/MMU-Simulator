# 🧠 MMU Simulator - Virtual Memory Management

A dual-interface **Memory Management Unit (MMU) Simulator** built in **C** (for command-line interaction) and **HTML/JavaScript** (for visual and interactive browser-based experience). This project simulates virtual-to-physical address translation using a **Page Table** and a **Translation Lookaside Buffer (TLB)**. It helps understand memory management concepts like TLB hits/misses, page faults, and access permissions.

---

## 📁 Project Structure

- mmu_simulator.c # CLI-based MMU Simulator (ARM64 assembly used for address breakdown)
- mmu_simulator.html # Browser-based GUI MMU Simulator

---

## 💻 Features

### C Version (CLI)

- Simulates virtual-to-physical memory translation
- Page Table with configurable permissions (Read, Write, Execute)
- TLB caching and flushing logic
- Page fault detection
- Real-time statistics: TLB hits, misses, page faults, translation count
- ARM64 inline assembly for address breakdown
- Colorful terminal output for better UX
- Commands:
  - `translate <address> <mode>` – Translate virtual address (`r/w/x`)
  - `add <v_page> <p_page> <perm>` – Add a new mapping (`rwx`)
  - `pagetable` – View page table
  - `tlb` – View TLB contents
  - `flushtlb` – Clear the TLB
  - `stats` – View MMU performance stats
  - `help` – Show command list
  - `exit` – Exit simulator

### HTML Version (GUI)

- Visually breakdown virtual address into Page Number and Offset
- GUI form to:
  - Add page mappings
  - Simulate translations
  - Flush the TLB
- Dynamic Page Table and TLB tables
- Event log system (with color-coded alerts)
- Live statistics dashboard (TLB hits, misses, page faults)
- Smooth animations and highlight effects
- Fully responsive UI

---

**Requirements:**
- ⚠️ Note: The C version uses ARM64 assembly and should be compiled/run in an ARM64 environment.
- GCC (ARM64 environment for inline assembly)

**Compile & Run:**
- gcc -o mmu_simulator mmu_simulator.c
- ./mmu_simulator
