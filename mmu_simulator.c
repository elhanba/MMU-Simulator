#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

// memory management constants
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define OFFSET_MASK 0xFFF
#define MAX_PAGE_TABLE_ENTRIES 100
#define TLB_SIZE 16

// access rights flags
#define READ_PERMISSION  0x1
#define WRITE_PERMISSION 0x2
#define EXEC_PERMISSION  0x4

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

typedef struct {
    uint64_t virtual_page;
    uint64_t physical_page;
    uint8_t permissions;  
    bool present;         
    bool dirty;           
    bool accessed;        
} page_table_entry_t;

typedef struct {
    uint64_t virtual_page;
    uint64_t physical_page;
    uint8_t permissions;
    bool valid;
} tlb_entry_t;

typedef struct {
    page_table_entry_t page_table[MAX_PAGE_TABLE_ENTRIES];
    int page_table_size;
    tlb_entry_t tlb[TLB_SIZE];
    int tlb_hits;
    int tlb_misses;
    int page_faults;
    int total_translations;
} mmu_t;

void mmu_init(mmu_t *mmu) {
    memset(mmu, 0, sizeof(mmu_t));
    
    mmu->page_table[0] = (page_table_entry_t){0x00000, 0x30000, READ_PERMISSION | WRITE_PERMISSION | EXEC_PERMISSION, true, false, false};
    mmu->page_table[1] = (page_table_entry_t){0x00001, 0x21000, READ_PERMISSION | WRITE_PERMISSION, true, false, false};
    mmu->page_table[2] = (page_table_entry_t){0x00002, 0x45000, READ_PERMISSION | EXEC_PERMISSION, true, false, false};
    mmu->page_table[3] = (page_table_entry_t){0x00003, 0x72000, READ_PERMISSION, true, false, false};
    mmu->page_table[4] = (page_table_entry_t){0x00004, 0x80000, READ_PERMISSION | WRITE_PERMISSION, true, false, false};
    mmu->page_table_size = 5;
    
    for (int i = 0; i < TLB_SIZE; i++) {
        mmu->tlb[i].valid = false;
    }
}

uint64_t get_page_number(uint64_t virtual_addr) {
    uint64_t page_number;
    
    __asm__ (
        "lsr %[result], %[addr], #12"
        : [result] "=r" (page_number)
        : [addr] "r" (virtual_addr)
    );
    
    return page_number;
}

uint64_t get_page_offset(uint64_t virtual_addr) {
    uint64_t offset;
    
    __asm__ (
        "and %[result], %[addr], #0xFFF"
        : [result] "=r" (offset)
        : [addr] "r" (virtual_addr)
    );
    
    return offset;
}

uint64_t calculate_physical_address(uint64_t phys_page, uint64_t offset) {
    uint64_t physical_addr;
    
    __asm__ (
        "lsl %[result], %[page], #12 \n\t"
        "orr %[result], %[result], %[off]"
        : [result] "=&r" (physical_addr)
        : [page] "r" (phys_page), [off] "r" (offset)
    );
    
    return physical_addr;
}

bool tlb_lookup(mmu_t *mmu, uint64_t page_number, uint64_t *physical_page, uint8_t *permissions) {
    for (int i = 0; i < TLB_SIZE; i++) {
        if (mmu->tlb[i].valid && mmu->tlb[i].virtual_page == page_number) {
            *physical_page = mmu->tlb[i].physical_page;
            *permissions = mmu->tlb[i].permissions;
            return true;
        }
    }
    return false;
}

void tlb_add(mmu_t *mmu, uint64_t virtual_page, uint64_t physical_page, uint8_t permissions) {
    static int next_entry = 0;
    
    mmu->tlb[next_entry].virtual_page = virtual_page;
    mmu->tlb[next_entry].physical_page = physical_page;
    mmu->tlb[next_entry].permissions = permissions;
    mmu->tlb[next_entry].valid = true;
    
    next_entry = (next_entry + 1) % TLB_SIZE;
}

void tlb_flush(mmu_t *mmu) {
    for (int i = 0; i < TLB_SIZE; i++) {
        mmu->tlb[i].valid = false;
    }
    printf(ANSI_COLOR_YELLOW "TLB has been flushed.\n" ANSI_COLOR_RESET);
}

int translate_address(mmu_t *mmu, uint64_t virtual_addr, uint64_t *physical_addr, uint8_t access_type) {
    mmu->total_translations++;
    
    uint64_t page_number = get_page_number(virtual_addr);
    uint64_t offset = get_page_offset(virtual_addr);
    uint64_t physical_page = 0;
    uint8_t permissions = 0;
    bool tlb_hit = false;
    
    printf("Virtual address 0x%llx breakdown:\n", virtual_addr);
    printf("  Page number: 0x%llx\n", page_number);
    printf("  Offset: 0x%llx\n", offset);
    
    if (tlb_lookup(mmu, page_number, &physical_page, &permissions)) {
        mmu->tlb_hits++;
        tlb_hit = true;
        printf(ANSI_COLOR_GREEN "TLB Hit! Found in TLB cache.\n" ANSI_COLOR_RESET);
    } else {
        mmu->tlb_misses++;
        printf(ANSI_COLOR_YELLOW "TLB Miss! Looking up in page table...\n" ANSI_COLOR_RESET);
        
        bool found = false;
        for (int i = 0; i < mmu->page_table_size; i++) {
            if (mmu->page_table[i].virtual_page == page_number && mmu->page_table[i].present) {
                physical_page = mmu->page_table[i].physical_page;
                permissions = mmu->page_table[i].permissions;
                mmu->page_table[i].accessed = true;
                
                tlb_add(mmu, page_number, physical_page, permissions);
                
                found = true;
                break;
            }
        }
        
        if (!found) {
            mmu->page_faults++;
            printf(ANSI_COLOR_RED "Page fault! Virtual page 0x%llx is not in the page table.\n" ANSI_COLOR_RESET, page_number);
            return 0;
        }
    }
    
    bool permission_ok = false;
    switch (access_type) {
        case READ_PERMISSION:
            permission_ok = (permissions & READ_PERMISSION) != 0;
            break;
        case WRITE_PERMISSION:
            permission_ok = (permissions & WRITE_PERMISSION) != 0;
            break;
        case EXEC_PERMISSION:
            permission_ok = (permissions & EXEC_PERMISSION) != 0;
            break;
    }
    
    if (!permission_ok) {
        printf(ANSI_COLOR_RED "Access violation! Insufficient permissions for the requested operation.\n" ANSI_COLOR_RESET);
        return -1;
    }
    
    if (access_type == WRITE_PERMISSION) {
        for (int i = 0; i < mmu->page_table_size; i++) {
            if (mmu->page_table[i].virtual_page == page_number) {
                mmu->page_table[i].dirty = true;
                break;
            }
        }
    }
    
    *physical_addr = calculate_physical_address(physical_page, offset);
    return 1;
}

bool add_page_mapping(mmu_t *mmu, uint64_t virtual_page, uint64_t physical_page, uint8_t permissions) {
    if (mmu->page_table_size >= MAX_PAGE_TABLE_ENTRIES) {
        return false;
    }
    
    for (int i = 0; i < mmu->page_table_size; i++) {
        if (mmu->page_table[i].virtual_page == virtual_page) {
            mmu->page_table[i].physical_page = physical_page;
            mmu->page_table[i].permissions = permissions;
            mmu->page_table[i].present = true;
            mmu->page_table[i].dirty = false;
            mmu->page_table[i].accessed = false;
            
            tlb_flush(mmu);
            return true;
        }
    }
    
    mmu->page_table[mmu->page_table_size].virtual_page = virtual_page;
    mmu->page_table[mmu->page_table_size].physical_page = physical_page;
    mmu->page_table[mmu->page_table_size].permissions = permissions;
    mmu->page_table[mmu->page_table_size].present = true;
    mmu->page_table[mmu->page_table_size].dirty = false;
    mmu->page_table[mmu->page_table_size].accessed = false;
    
    mmu->page_table_size++;
    return true;
}

void display_page_table(mmu_t *mmu) {
    printf(ANSI_COLOR_CYAN "\n===== PAGE TABLE =====\n" ANSI_COLOR_RESET);
    printf("| %-12s | %-12s | %-12s | %-7s | %-7s | %-7s |\n", 
           "Virtual Page", "Physical Page", "Permissions", "Present", "Dirty", "Accessed");
    printf("|--------------|--------------|--------------|---------|---------|----------|\n");
    
    for (int i = 0; i < mmu->page_table_size; i++) {
        page_table_entry_t *entry = &mmu->page_table[i];
        
        char perm_str[4] = "---";
        if (entry->permissions & READ_PERMISSION) perm_str[0] = 'r';
        if (entry->permissions & WRITE_PERMISSION) perm_str[1] = 'w';
        if (entry->permissions & EXEC_PERMISSION) perm_str[2] = 'x';
        
        printf("| 0x%-10llx | 0x%-10llx | %-12s | %-7s | %-7s | %-7s |\n",
               entry->virtual_page, entry->physical_page, perm_str,
               entry->present ? "Yes" : "No",
               entry->dirty ? "Yes" : "No",
               entry->accessed ? "Yes" : "No");
    }
    printf("\n");
}

void display_tlb(mmu_t *mmu) {
    printf(ANSI_COLOR_CYAN "\n===== TLB CONTENTS =====\n" ANSI_COLOR_RESET);
    printf("| %-5s | %-12s | %-12s | %-12s |\n", 
           "Index", "Virtual Page", "Physical Page", "Permissions");
    printf("|-------|--------------|--------------|----------------|\n");
    
    for (int i = 0; i < TLB_SIZE; i++) {
        tlb_entry_t *entry = &mmu->tlb[i];
        
        if (entry->valid) {
            char perm_str[4] = "---";
            if (entry->permissions & READ_PERMISSION) perm_str[0] = 'r';
            if (entry->permissions & WRITE_PERMISSION) perm_str[1] = 'w';
            if (entry->permissions & EXEC_PERMISSION) perm_str[2] = 'x';
            
            printf("| %-5d | 0x%-10llx | 0x%-10llx | %-12s |\n",
                   i, entry->virtual_page, entry->physical_page, perm_str);
        } else {
            printf("| %-5d | %12s | %12s | %12s |\n", i, "INVALID", "-", "-");
        }
    }
    printf("\n");
}

void display_stats(mmu_t *mmu) {
    printf(ANSI_COLOR_CYAN "\n===== MMU STATISTICS =====\n" ANSI_COLOR_RESET);
    printf("Total address translations: %d\n", mmu->total_translations);
    printf("TLB hits: %d (%.2f%%)\n", mmu->tlb_hits, 
           mmu->total_translations > 0 ? (float)mmu->tlb_hits / mmu->total_translations * 100 : 0);
    printf("TLB misses: %d (%.2f%%)\n", mmu->tlb_misses, 
           mmu->total_translations > 0 ? (float)mmu->tlb_misses / mmu->total_translations * 100 : 0);
    printf("Page faults: %d\n", mmu->page_faults);
    printf("\n");
}

void clear_screen() {
    printf("\033[H\033[J");
}

void display_help() {
    printf(ANSI_COLOR_CYAN "\n===== MMU SIMULATOR HELP =====\n" ANSI_COLOR_RESET);
    printf("Available commands:\n");
    printf("  translate <address> <mode>  - Translate virtual address to physical (mode: r/w/x)\n");
    printf("  add <v_page> <p_page> <perm> - Add a new page mapping (perm: rwx)\n");
    printf("  pagetable                   - Display the page table\n");
    printf("  tlb                         - Display the TLB\n");
    printf("  flushtlb                    - Flush the TLB\n");
    printf("  stats                       - Display statistics\n");
    printf("  clear                       - Clear the screen\n");
    printf("  help                        - Display this help message\n");
    printf("  exit                        - Exit the simulator\n\n");
}

int main() {
    mmu_t mmu;
    char command[256];
    char cmd[32], arg1[32], arg2[32], arg3[32];
    uint64_t virtual_addr, physical_addr;
    uint64_t v_page, p_page;
    uint8_t permissions, access_type;
    
    mmu_init(&mmu);
    clear_screen();
    printf(ANSI_COLOR_MAGENTA "======================================\n");
    printf("     MMU SIMULATOR - ARM64 Edition\n");
    printf("======================================\n" ANSI_COLOR_RESET);
    printf("Type 'help' for a list of commands.\n\n");
    
    while (1) {
        printf(ANSI_COLOR_GREEN "mmu> " ANSI_COLOR_RESET);
        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }
        
        cmd[0] = arg1[0] = arg2[0] = arg3[0] = '\0';
        sscanf(command, "%31s %31s %31s %31s", cmd, arg1, arg2, arg3);
        
        if (strcmp(cmd, "exit") == 0) {
            break;
        }
        else if (strcmp(cmd, "help") == 0) {
            display_help();
        }
        else if (strcmp(cmd, "clear") == 0) {
            clear_screen();
        }
        else if (strcmp(cmd, "translate") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0') {
                printf("Error: Missing arguments. Usage: translate <address> <mode>\n");
                continue;
            }
            
            if (strncmp(arg1, "0x", 2) == 0) {
                sscanf(arg1, "0x%llx", &virtual_addr);
            } else {
                sscanf(arg1, "%llx", &virtual_addr);
            }
            
            switch (arg2[0]) {
                case 'r': access_type = READ_PERMISSION; break;
                case 'w': access_type = WRITE_PERMISSION; break;
                case 'x': access_type = EXEC_PERMISSION; break;
                default:
                    printf("Error: Invalid access mode. Use 'r' for read, 'w' for write, or 'x' for execute.\n");
                    continue;
            }
            
            printf("\nTranslating virtual address 0x%llx with %s access...\n", 
                   virtual_addr, 
                   access_type == READ_PERMISSION ? "read" : 
                   access_type == WRITE_PERMISSION ? "write" : "execute");
                   
            int result = translate_address(&mmu, virtual_addr, &physical_addr, access_type);
            if (result == 1) {
                printf(ANSI_COLOR_GREEN "Success! Virtual address 0x%llx maps to physical address 0x%llx\n" ANSI_COLOR_RESET, 
                       virtual_addr, physical_addr);
            }
        }
        else if (strcmp(cmd, "add") == 0) {
            if (arg1[0] == '\0' || arg2[0] == '\0' || arg3[0] == '\0') {
                printf("Error: Missing arguments. Usage: add <v_page> <p_page> <perm>\n");
                continue;
            }
            
            if (strncmp(arg1, "0x", 2) == 0) {
                sscanf(arg1, "0x%llx", &v_page);
            } else {
                sscanf(arg1, "%llx", &v_page);
            }
            
            if (strncmp(arg2, "0x", 2) == 0) {
                sscanf(arg2, "0x%llx", &p_page);
            } else {
                sscanf(arg2, "%llx", &p_page);
            }
            
            permissions = 0;
            for (size_t i = 0; i < strlen(arg3); i++) {
                switch (arg3[i]) {
                    case 'r': permissions |= READ_PERMISSION; break;
                    case 'w': permissions |= WRITE_PERMISSION; break;
                    case 'x': permissions |= EXEC_PERMISSION; break;
                }
            }
            
            if (add_page_mapping(&mmu, v_page, p_page, permissions)) {
                printf(ANSI_COLOR_GREEN "Successfully added mapping: Virtual page 0x%llx -> Physical page 0x%llx with permissions %s\n" ANSI_COLOR_RESET,
                       v_page, p_page, arg3);
            } else {
                printf(ANSI_COLOR_RED "Failed to add mapping. Page table might be full.\n" ANSI_COLOR_RESET);
            }
        }
        else if (strcmp(cmd, "pagetable") == 0) {
            display_page_table(&mmu);
        }
        else if (strcmp(cmd, "tlb") == 0) {
            display_tlb(&mmu);
        }
        else if (strcmp(cmd, "flushtlb") == 0) {
            tlb_flush(&mmu);
        }
        else if (strcmp(cmd, "stats") == 0) {
            display_stats(&mmu);
        }
        else if (cmd[0] != '\0') {
            printf("Unknown command: %s. Type 'help' for available commands.\n", cmd);
        }
    }
    
    printf("Exiting MMU Simulator.\n");
    return 0;
}