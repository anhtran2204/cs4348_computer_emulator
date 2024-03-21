#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <unistd.h>

#define PERROR(FMT,...) \
  fprintf(stderr, FMT ": %s\n", ##__VA_ARGS__, strerror(errno))

using namespace std;

int *load_data(const char *file_name);

int main(int argc, const char *argv[]) {
    int mem_to_cpu[2];
    int cpu_to_mem[2];
    pid_t pid;

    const char *file_name = argv[1];
    int timer = atoi(argv[2]);
//    int timer = 30;

    int result1 = pipe(mem_to_cpu);
    int result2 = pipe(cpu_to_mem);

    if (result1 < 0 || result2 < 0) {
        printf("pipe() failed");
        exit(1);
    }

//   const char *file_name = "sample5.txt";
//    load_data(file_name);

    int buf[100];
    fflush(0);
    pid = fork();
    if (pid < 0) {
        printf("fork() failed!");
        exit(1);
    }
    else if (pid == 0) {
        // Memory process
        int *memory = load_data(file_name);
        int PC, data, address;
        PC = 0;
        data = 0;
        address = 0;
        const int write_flag = -1;
        const int kill_child = -2;

        while (true) {
            if (PC == kill_child) {
                _exit(0);
            }
            read(cpu_to_mem[0], &PC, sizeof(PC));
            if (PC == write_flag) {         // Writing to the memory
                read(cpu_to_mem[0], &address, sizeof(address));
                read(cpu_to_mem[0], &data, sizeof(data));
                memory[address] = data;
            } else {                        // Reading from the memory 
                int instruction = memory[PC];
                write(mem_to_cpu[1], &instruction, sizeof(instruction));
            }
        }
    } else {
        // CPU process
        int PC, SP, IR, AC, X, Y;
        int tempReg = 0;
        int operand = 0;

        X = 0;
        Y = 0;
        IR = 0;
        AC = 0;
        SP = 1000;
        PC = 0;

        bool kernel_mode = false;   // true: kernel mode; false: user mode
        int interrupt_flag = 0;     // 0: no interrupt; 1: int insutrction; 2: timer interrupt
        bool timer_interrupt_flag = false;
        int timer_counter = 0;

        const int write_flag = -1;  // flag to signify writing to the memory
        const int kill_child = -2;  // flag to notify memory to terminate all child processes
        const int INT_INTERRUPT_ADDR = 1499;

        while (true) {
            // Timer Interrupt Handler
            if (timer_interrupt_flag && interrupt_flag == 0) {
                timer_interrupt_flag = false;
                interrupt_flag = 2;

                kernel_mode = true;
                tempReg = SP;
                SP = 2000;

                SP--;
                PC++;
                write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                write(cpu_to_mem[1], &SP, sizeof(SP));
                write(cpu_to_mem[1], &PC, sizeof(PC));

                SP--;
                write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                write(cpu_to_mem[1], &SP, sizeof(SP));
                write(cpu_to_mem[1], &tempReg, sizeof(tempReg));
                PC = 1000;
            }

            // Read from the memory to get the instructions
            write(cpu_to_mem[1], &PC, sizeof(PC));
            read(mem_to_cpu[0], &IR, sizeof(IR));
            switch (IR) {
                case 1:         // Load value into AC
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    AC = operand;
                    break;

                case 2:         // Load from address given into AC
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    if (operand >= 1000 && !kernel_mode) {
                        printf("Memory violation: accessing system address %d in user mode\n", operand);
                        exit(1);
                    }
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    AC = operand;
                    break;

                case 3: {       // Load from address found in given address into AC
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    if (operand >= 1000 && !kernel_mode) {
                        printf("Memory violation: accessing system address %d in user mode\n", operand);
                        exit(1);
                    }
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    AC = operand;
                    break;
                }

                case 4: {       // Load value at (addr + X) into AC
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    operand += X;
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    AC = operand;
                    break;
                }

                case 5: {       // Load value at (addr + Y) into AC
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    operand += Y;
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    AC = operand;
                    break;
                }

                case 6: {       //  Load from (SP + X) into AC
                    operand = SP + X;
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    AC = operand;
                    break;
                }

                case 7: {       // Store addr: Store the value in the AC into the address?? Need work
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                    write(cpu_to_mem[1], &operand, sizeof(operand));
                    write(cpu_to_mem[1], &AC, sizeof(AC));
                    break;
                }

                case 8: {       // Get a random int from 1-100 into AC
                    AC = rand() % 100 + 1;
                    break;
                }

                case 9: {       // If operand == 1, print AC as int; if operand == 2, print as char
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    if (operand == 1) {
                        printf("%i", AC);
                    } else if (operand == 2) {
                        printf("%c", AC);
                    }
                    break;
                }

                case 10:        // Add X to AC
                    AC += X;
                    break;

                case 11:        // Add Y to AC
                    AC += Y;
                    break;

                case 12:        // Subtract X from AC
                    AC -= X;
                    break;

                case 13:        // Subtract X from AC
                    AC -= Y;
                    break;

                case 14:        // Copy value of AC to X
                    X = AC;
                    break;

                case 15:
                    AC = X;     // Copy value of X to AC
                    break;

                case 16:
                    Y = AC;     // Copy value of AC to Y
                    break;

                case 17:        // Copy value of Y to AC
                    AC = Y;
                    break;

                case 18:        // Copy value of AC to SP
                    SP = AC;
                    break;

                case 19:        // Copy value of SP to AC
                    AC = SP;
                    break;

                case 20: {      // Jump to address given
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    PC = operand + write_flag;
                    break;
                }

                case 21:        // Jump to address given if AC is zero
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    if (AC == 0) {
                        PC = operand + write_flag;
                    }
                    break;

                case 22:        // Jump to address given if AC is not zero
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    if (AC != 0) {
                        PC = operand + write_flag;
                    }
                    break;

                case 23:        // Push return address onto stack, jump to the address
                    PC++;
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    read(mem_to_cpu[0], &operand, sizeof(operand));
                    SP--;
                    PC++;
                    write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    write(cpu_to_mem[1], &PC, sizeof(PC));
                    PC = operand + write_flag;
                    break;

                case 24:        // Pop return address from stack, jump to the address
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    read(mem_to_cpu[0], &PC, sizeof(PC));
                    PC--;
                    SP++;
                    break;

                case 25:        // Increment X
                    X++;
                    break;

                case 26:        // Decrement X
                    X--;
                    break;

                case 27:        // Push value of AC onto stack
                    SP--;
                    write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    write(cpu_to_mem[1], &AC, sizeof(AC));
                    break;

                case 28:        // Pop from stack into AC
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    read(mem_to_cpu[0], &AC, sizeof(AC));
                    SP++;
                    break;

                case 29:        // Perform system call
                    if (interrupt_flag != 0) {
                        break;
                    }
                    interrupt_flag = 1;

                    kernel_mode = true;

                    tempReg = SP;
                    SP = 2000;

                    SP--;
                    write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    write(cpu_to_mem[1], &PC, sizeof(PC));

                    SP--;
                    write(cpu_to_mem[1], &write_flag, sizeof(write_flag));
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    write(cpu_to_mem[1], &tempReg, sizeof(tempReg));

                    PC = INT_INTERRUPT_ADDR;
                    break;

                case 30:        // Return from system call
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    read(mem_to_cpu[0], &tempReg, sizeof(tempReg));
                    SP++;
                    write(cpu_to_mem[1], &SP, sizeof(SP));
                    read(mem_to_cpu[0], &PC, sizeof(PC));
                    PC -= 2;
                    SP++;

                    if (interrupt_flag == 2) {
                        timer_interrupt_flag = false;
                    }
                    interrupt_flag = 0;
                    SP = tempReg;
                    kernel_mode = false;
                    break;

                case 50:        // End program
                    write(cpu_to_mem[1], &kill_child, sizeof(kill_child));
                    exit(0);

                default:        // Invalid instruction
                    printf("Error: %d not an instruction!\n", IR);
            }
            timer_counter++;
            PC++;

            // Validify if timer flag is true
            if (timer_counter % timer == 0) {
                timer_interrupt_flag = true;
            }
        }
    }
    return 0;
}

/*  Load the instructions and data 
    from the input files into the memory*/
int *load_data(const char *file_name) {
    static int memory[2000];    // Memory
    char buf[1000];             // Buffer
    int line = 0;               

    FILE *file;
    file = fopen(file_name, "r");

    if (!file) {
        printf("The file %s could not be opened", file_name);
        return memory;
    }

//    int size = sizeof(memory) / sizeof(memory[0]);
//    for (int i = 0; i < size; ++i) {
//        cout << memory[i] << endl;
//    }

    while(fgets(buf, sizeof(buf), file) != NULL) {
        // Check for address for system call and interrupt and its handler
        // If starts with '.', load the instructions that handle this handler
        // into the system portion of the memory
        if (isdigit((int) buf[0]) || buf[0] == '.') { 
            int num;
            if (buf[0] == '.') {
                sscanf(buf, ".%d %*s\n", &num);
                line = num;
                memory[line] = num;
            } else {
                sscanf(buf, "%d %*s\n", &num);
                memory[line] = num;
                line++;
            }
        }
    }
    fclose(file);
    return memory;
}
