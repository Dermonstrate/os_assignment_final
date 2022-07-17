#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

/* 全局变量 */
#define PAGE_SIZE 256 // 页面大小
#define PAGE_ENTRIES 256 // 页表最大条目数
#define PAGE_NUM_BITS 8 // 页码大小
#define FRAME_SIZE 256 // 帧大小
#define FRAME_ENTRIES 256 // 物理地址帧数
#define MEM_SIZE (FRAME_SIZE * FRAME_ENTRIES) // 内存大小
#define TLB_ENTRIES 16 // TLB条目数

int virtual; // 虚拟地址
int page_number; // 页数
int offset; 
int physical; // 物理地址
int frame_number; // 帧数
int value;
int page_table[PAGE_ENTRIES]; // 页表
int tlb[TLB_ENTRIES][2]; // TLB
int tlb_front = -1; // TLB前标号
int tlb_back = -1; // TLB后标号
int tlb_lru[TLB_ENTRIES] = {0}; //判断TLB频率
char memory[MEM_SIZE]; // 物理内存
int mem_index = 0; // 初始帧
int mem_indexx = 0; //初始帧2
int fault_counter = 0; // 错页计数
int tlb_counter = 0; // TLB命中计数
int address_counter = 0; // 文件条数计数
float fault_rate; // 错误率
float tlb_rate; // TLB命中率
int page_lru[PAGE_ENTRIES] = {0}; //页表使用时间
int count_all = 0; //用作LRU算法的时钟

/*函数 */
int get_page_number(int virtual) //返回页号
{
    return (virtual >> PAGE_NUM_BITS);
}

int get_offset(int virtual) //返回偏移量
{
    int mask = 255;
    return virtual & mask;
}

int get_physical(int virtual) //返回物理地址
{
    physical = get_page_number(virtual) + get_offset(virtual);
    return physical;
}

void initialize_page_table(int n) //初始化页表
{
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        page_table[i] = n;
    }
}

void initialize_tlb(int n) //初始化TLB
{
    for (int i = 0; i < TLB_ENTRIES; i++) {
        tlb[i][0] = -1;
        tlb[i][1] = -1;
    }
}

int consult_page_table(int page_number) //检验页数帧数
{
    if (page_table[page_number] == -1) {
        fault_counter++;
    }
    return page_table[page_number];
}


int consult_tlb(int page_number) //检验TLB
{
    for (int i = 0; i < TLB_ENTRIES; i++) {
        if (tlb[i][0] == page_number) {
            /* TLB 命中! */
            tlb_counter++;
            tlb_lru[i] = count_all;
            return tlb[i][1];
        }
    }
    return -1;
}

void update_tlb(int page_number, int frame_number) //更新TLB，使用LRU策略
{
    if (tlb_front == -1) {
        /* 若TLB为空 */
        tlb_front = 0;
        tlb_back = 0; 

        tlb[tlb_back][0] = page_number;
        tlb[tlb_back][1] = frame_number;
    }
    else {
        int tlb_lru_ini = tlb_lru[0];
        int count_lru = 0;
        int i = 0;
        for(i=0;i<16;i++)
	{
	if(tlb_lru_ini>tlb_lru[i])
	        {
	        tlb_lru_ini = tlb_lru[i]; //取时钟最小者
	        count_lru = i;
	        }
	}
        tlb_lru[count_lru] = count_all; //更新时钟
        tlb_front = count_lru;
        tlb_back = count_lru;
        
        tlb[tlb_back][0] = page_number;
        tlb[tlb_back][1] = frame_number;
    }
}

int main(int argc, char *argv[]) {
    char* in_file; 
    char* out_file; 
    char* store_file; 
    char* store_data; 
    int store_fd; 
    char line[8]; 
    FILE* in_ptr; 
    FILE* out_ptr; 

    /* 初始化页表 */
    initialize_page_table(-1);
    initialize_tlb(-1);

    if (argc != 4) {
        printf("Enter input, output, and store file names!");
        exit(EXIT_FAILURE);
    }
    else {
        /* 获取文件名 */
        in_file = argv[1];
        out_file = argv[2];
        store_file = argv[3];

        /* 打开文件 */
        if ((in_ptr = fopen(in_file, "r")) == NULL) {
            /* If fopen fails, print error and exit. */
            printf("Input file could not be opened.\n");
            exit(EXIT_FAILURE);
        }

        /* 打开输出文件 */
        if ((out_ptr = fopen(out_file, "a")) == NULL) {
            /* If fopen fails, print error and exit. */
            printf("Output file could not be opened.\n");
            exit(EXIT_FAILURE);
        }

        store_fd = open(store_file, O_RDONLY);
        store_data = mmap(0, MEM_SIZE, PROT_READ, MAP_SHARED, store_fd, 0);
        if (store_data == MAP_FAILED) { //检查mmap
            close(store_fd);
            printf("Error mmapping the backing store file!");
            exit(EXIT_FAILURE);
        }

        /* 开始循环 */
        while (fgets(line, sizeof(line), in_ptr)) {
            virtual = atoi(line);
            address_counter++;
            count_all++;
            page_number = get_page_number(virtual); //从虚拟地址获取页数
            offset = get_offset(virtual); //获取偏移量

            frame_number = consult_tlb(page_number); //寻找TLB中帧数

            if (frame_number != -1) {
                physical = frame_number + offset;
                value = memory[physical];
            }
            else {
                frame_number = consult_page_table(page_number);
                if (frame_number != -1) {
	    /* TLB中无对应*/
	    page_lru[page_number] = count_all;
                    physical = frame_number + offset;
                    update_tlb(page_number, frame_number);// 更新TLB，使用FIFO策略
                    value = memory[physical];
                }
                else {
                    /* 出现错页 */
                    int page_address = page_number * PAGE_SIZE;

                    /* 检查是否存在空帧 */ 
                    if (mem_index != -1) {
                        memcpy(memory + mem_index, 
                               store_data + page_address, PAGE_SIZE);

                        frame_number = mem_index; // 计算物理地址 
                        physical = frame_number + offset;
                        value = memory[physical]; // 取值

                        page_table[page_number] = mem_index;// 更新页表
	        page_lru[page_number]++;
                        update_tlb(page_number, frame_number);// 更新TLB

                        if (mem_index < MEM_SIZE - FRAME_SIZE)// 增加序号
                        {
                            mem_index += FRAME_SIZE;
                        }
                        else {
	            mem_indexx = mem_index;
                            mem_index = -1; //不存在空帧
	            fprintf(out_ptr, "空帧用尽\n");
                        }
	   }
	   else{
		/* 若不存在空帧 */
		/* LRU算法实现页面置换*/
	                int page_lru_init = page_lru[0];
	                int count_p_lru = 0;
       		for(int i=0;i<256;i++)
		{
		       if(page_lru_init>page_lru[i])
	         		{
	         		page_lru_init = page_lru[i]; //按时钟先后取最早被使用
	         		count_p_lru = i;
	         		}
		} 
		mem_indexx = FRAME_SIZE * count_p_lru; //寻找牺牲页
		memcpy(memory + mem_indexx, 
                     		store_data + page_address, PAGE_SIZE); //将牺牲页进行替换
       		frame_number = mem_indexx; //从内存中寻找所需页
       		physical = frame_number + offset;
      		value = memory[physical]; //取值 
		page_lru[count_p_lru] = count_all; // 更新时间 
       		page_table[page_number] = frame_number;// 更新页表
                                update_tlb(page_number, frame_number);// 更新TLB	
	         }
                   }
            }

            fprintf(out_ptr, "Virtual address: %d ", virtual); 
            fprintf(out_ptr, "Physical address: %d ", physical);
            fprintf(out_ptr, "Value: %d\n", value);
        }

        /* 计算概率 */
        fault_rate = (float) fault_counter / (float) address_counter;
        tlb_rate = (float) tlb_counter / (float) address_counter;

        /* 按格式输出内容 */
        fprintf(out_ptr, "Number of Translated Addresses = %d\n", address_counter); 
        fprintf(out_ptr, "Page Faults = %d\n", fault_counter);
        fprintf(out_ptr, "Page Fault Rate = %.3f\n", fault_rate);
        fprintf(out_ptr, "TLB Hits = %d\n", tlb_counter);
        fprintf(out_ptr, "TLB Hit Rate = %.3f\n", tlb_rate);

        fclose(in_ptr);
        fclose(out_ptr);
        close(store_fd);
    }

    return EXIT_SUCCESS;
}
