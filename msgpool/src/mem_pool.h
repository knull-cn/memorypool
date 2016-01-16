
#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>


//struct AllocInfo
//{
//    int32_t sz_alloced_;//已经分配出去给msg用的内存大小;
//    int32_t sz_freed_;//分配出去,又释放回来的内存大小;
//    int16_t cnt_alloced_;//分配出去的个数;
//    int16_t cnt_freed_;//分配出去,又释放回来的个数;
//    int32_t alloc_pos_;
//};

struct PoolBlockHeader
{
    int32_t ref_;//id;
    int32_t sys_alloc_size_;//本内存大小,包括自己;
    int16_t sys_alloc_type_;//1-system alloc;other:pool alloc;
    int16_t sys_alloc_state_;//AS_FREED(0)-in freed_ queue;AS_ALLOCING(1)-in allocing_ queue;AS_FREEING(3):in freeing_ queue;
    //AllocInfo alloc_info_;
    int16_t cnt_alloced_;//分配出去的个数;
    int16_t cnt_freed_;//分配出去,又释放回来的个数;
    int32_t sz_alloced_;//已经分配出去给msg用的内存大小;
    int32_t sz_freed_;//分配出去,又释放回来的内存大小;
    int32_t pos_alloced_;//下一个分配出去内存的pos.和sz_alloced_存在一个PoolBlockHeader距离.
    int32_t reserved_;
};

struct BlockHeader
{
    int32_t diff_;
    int32_t size_;
    //int16_t status_;
    //int32_t reserve_;
};

//单线程.
class MsgMemPool
{
public :
    MsgMemPool();
    ~MsgMemPool();
    void *MAlloc(int32_t sz);
    void MFree(void *mem);
private :
    void *SystemAlloc(int32_t sz);
    void SystemFree(PoolBlockHeader* ppbh);
    void *PoolAlloc(int32_t sz);
    PoolBlockHeader* GetFromAllocing(int32_t need_size);
    PoolBlockHeader* GetFromFreed();
    void PoolFree(PoolBlockHeader* ppbh,BlockHeader* pbh);
private :
    void MoveToFree(std::vector<void*> &temp);
    void NeedMove(int32_t left_size,void *mem,std::vector<void*> &temp);
    void InitMemPoolBlockHeader(void *mem,int32_t ref);
    //void InitMemBlockHeader(void *mem,int32_t diff,int32_t size);
    void Resize();
    inline void *MoveForward(void *m,int32_t diff);
    inline void *MoveBackward(void *m,int32_t diff);
    void FreeToSystem();
private :
    std::vector<void* > mem_pool_;
    //
    std::vector<void *> freed_;
    typedef std::unordered_set<void *>::iterator MItr;
    std::unordered_set<void *> allocing_;
    std::unordered_set<void *> freeing_;
};
//-------------------------------------------------------------------
//inline function;
void *MsgMemPool::MoveForward(void *m,int32_t diff)
{
    return (void*)(static_cast<char*>(m) - diff);
}

void *MsgMemPool::MoveBackward(void *m,int32_t diff)
{
    return (void*)(static_cast<char*>(m) + diff);
}



//void MsgMemPool::InitMemBlockHeader(void *mem,int32_t diff,int32_t size)
//{
//    BlockHeader* pbh = reinterpret_cast<BlockHeader*>(mem);
//    pbh->diff_ = (diff);
//    pbh->size_ = (size);
//    //pbh->status_ = 0;
//    //pbh->reserve_ = 0;
//}

























