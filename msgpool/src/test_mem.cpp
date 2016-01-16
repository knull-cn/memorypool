/*
 * test_mem.cpp
 *
 *  Created on: 2015年12月5日
 *      Author: knull
 */


#include <time.h>
#include <cstdlib>
#include <cstdio>
#include <thread>
#include <vector>
#include <random>
#include "typecast.h"
#include "mem_pool.h"

using namespace std;

const int64_t MAX_ALLOC_SIZE_THRESHOLD = 2*1024*1024*1024ll;//2G;
std::vector<void*> g_record;
int32_t times = 1024*1024;
int32_t TEST_MAX_ALLOC_SIZE = 128*1024;


//1-base test;
void test_mempool(int32_t sz,MsgMemPool &pool)
{
    void *p = pool.MAlloc(sz);
    pool.MFree(p);
}

const int32_t TSIZE64K = 64 * 1024;
void test1()
{
    MsgMemPool pool;
    test_mempool(7,pool);
    test_mempool(8,pool);
    test_mempool(9,pool);

    test_mempool(TSIZE64K - 1,pool);
    test_mempool(TSIZE64K,pool);
    test_mempool(TSIZE64K + 1,pool);
}
//2-多线程测试
void systemalloc()
{
    int64_t memsize = 0;
    size_t pos = 0;
    int32_t alloc_size = 0;
    for (int32_t i=0; i!=times;++i)
    {
        alloc_size = rand()%TEST_MAX_ALLOC_SIZE;
        g_record.push_back(malloc(UINT32(alloc_size)));
        memsize += alloc_size;
        if (memsize > MAX_ALLOC_SIZE_THRESHOLD)
        {
            break;
        }
        if (i % 100 == 0)
        {
            pos = rand()%g_record.size();
            if (g_record.at(pos))
            {
                free(g_record.at(pos));
            }
            g_record.at(pos) = NULL;
        }
    }

    for (size_t i=0; i!=g_record.size();++i)
    {
        if (g_record.at(i))
        {
            free(g_record.at(i));
        }
    }
    printf("has alloc/free %llu'times\n",g_record.size());
    g_record.clear();
}

void poolalloc()
{
    int64_t memsize = 0;
    static MsgMemPool pool_;
    size_t pos = 0;
    int32_t alloc_size = 0;
    for (int32_t i=0; i!=times;++i)
    {
        alloc_size = rand()%TEST_MAX_ALLOC_SIZE;
        g_record.push_back(pool_.MAlloc(alloc_size));
        memsize += alloc_size;
        if (memsize > MAX_ALLOC_SIZE_THRESHOLD)
        {
            break;
        }
        if (i % 100 == 0)
        {
            pos = rand()%g_record.size();
            if (g_record.at(pos))
            {
                pool_.MFree(g_record.at(pos));
            }
            g_record.at(pos) = NULL;
        }
    }

    for (size_t i=0; i!=g_record.size();++i)
    {
        if (g_record.at(i))
        {
            pool_.MFree(g_record.at(i));
        }
    }
    printf("has alloc/free %llu'times\n",g_record.size());
    g_record.clear();
}

void Usage(const char *prog)
{
    printf("%s type thr_num\n",prog);
    printf("\ttype: 1 system; 2 memory-pool\n");
    printf("\tthr_num:number of thread\n");
}

typedef std::function<void()> ThrFun;
ThrFun g_thr_func;

void run_test(int32_t type,int32_t thr_num)
{
    if (type == 1)
    {
        g_thr_func = systemalloc;
    }
    else
    {
        g_thr_func = poolalloc;
    }

    std::vector<std::thread> gthreds_;
    for (int32_t i=0; i!=thr_num;++i)
    {
        gthreds_.push_back(move(thread(g_thr_func)));
    }
    for (int32_t i=0; i!=thr_num;++i)
    {
        gthreds_.at(i).join();
    }
}

void test2_Linux(int argc,char *argv[])
{    
    if (argc != 3)
    {
        Usage(argv[0]);
        return ;
    }
    int32_t type = atoi(argv[1]);
    int32_t thr_num = atoi(argv[2]);

    clock_t start = clock();
    run_test(type,thr_num);
    clock_t end = clock();

    if (type == 1)
    {
        printf("systemalloc :\n");
    }
    else
    {
        printf("poolalloc :\n");
    }
    printf("%d thread;alloc/free %d times(per thr);cost %lld clock_t;\n",thr_num,times,end-start);
}

void test2_win(int32_t type,int32_t thr_num)
{
    //clock_t start = clock();
    time_t start = time(0);
    run_test(type, thr_num);
    run_test(type, thr_num);
    run_test(type, thr_num);
    run_test(type, thr_num);
    //clock_t end = clock();
    time_t end = time(0);

    if (type == 1)
    {
        printf("systemalloc :\n");
    }
    else
    {
        printf("poolalloc :\n");
    }
    printf("%d thread;alloc/free %d times(per thr);cost %lld clock_t;\n", thr_num, times, end - start);
}

void test2(int argc,char *argv[])
{
#ifdef WIN32
    //test2_win(1,1);
    test2_win(2,1);
#else
    test2_Linux(argc,argv);
#endif
}


int main(int argc,char *argv[])
{
    srand(UINT32(time(0)));
    test1();
    test2(argc,argv);
    return 0;
}






















