#include <cstring>
#include <cassert>

#include "typecast.h"
#include "qmemory.h"
#include "mem_pool.h"

using std::vector;
//-------------------------------------------------------------------------------------------
const int32_t PBHEADER_SIZE = sizeof(PoolBlockHeader);
const int32_t BHEADER_SIZE = sizeof(BlockHeader);
const int32_t FREE_MIN_SIZE = 32;
const int32_t FREE_GUARD_SIZE = 128;
const int32_t POOL_ONE_TIME_ALLOC_MAX_SIZE = 64*1024;
const int32_t ONE_TIME_PAGE_MOVE_NUM = 10;//内存池block,从allocing到freeing，一次最多操作的次数.

const int32_t ONE_PAGE_IN_POOL = 1*1024*1024;//1M;
const int32_t ONE_TIME_ALLOC_NUMBER = 50;//50;


enum ALLOCTYPE
{
    AT_SYSTEM = 0,//alloc from system ;
    AT_POOL   = 1,//ALLOC from this pool;
};

enum ALLOCSTATE
{
    AS_FREED = 0,//空闲状态(MsgMemPool::freed_);
    AS_ALLOCING = 1,//在分配队列中(MsgMemPool::allocing_);有内存已经分配出去了;并且，有足够的内存继续分配给其他需要的;
    AS_FREEING = 3,//在释放队列中(MsgMemPool::freeing_);不会再分配内存出去,只会回收内存;所以内存回收,则放回到freed队列中去.
};
//-------------------------------------------------------------------------------------------
//public function;---------------------------------------------------------------------------
MsgMemPool::MsgMemPool()
{
    mem_pool_.reserve(1024);
    freed_.reserve(1024);
    allocing_.reserve(1024);
    freeing_.reserve(1024);
    Resize();
}

MsgMemPool::~MsgMemPool()
{
    assert(allocing_.empty());
    assert(freeing_.empty());
    freed_.clear();

    size_t psz = mem_pool_.size();
    size_t pos = 0;
    while (pos != psz)
    {
        qfree(mem_pool_.at(pos));
        mem_pool_.at(pos) = NULL;
        ++pos;
    }
    mem_pool_.clear();
}

void* MsgMemPool::MAlloc(int32_t sz)
{
    //Account(sz);
    //
    if (sz > POOL_ONE_TIME_ALLOC_MAX_SIZE)
    {
        return SystemAlloc(sz);
    }
    //
    return PoolAlloc(sz);
}

void MsgMemPool::MFree(void *mem)
{
    void *psrc = MoveForward(mem,BHEADER_SIZE);
    BlockHeader* pbh = reinterpret_cast<BlockHeader*>(psrc);
    PoolBlockHeader* ppbh = reinterpret_cast<PoolBlockHeader*>(MoveForward(psrc,pbh->diff_));
    switch (ppbh->sys_alloc_type_)
    {
    case AT_SYSTEM:
    {
        SystemFree(ppbh);
        break;
    }
    case AT_POOL:
    {
        PoolFree(ppbh,pbh);
        break;
    }
    default :
        assert(0);
    }
}

//private function;---------------------------------------------------------------------------
//1-sys alloc/freee;
void* MsgMemPool::SystemAlloc(int32_t sz)
{
    int32_t alloc_size = sz + PBHEADER_SIZE + BHEADER_SIZE;
    alloc_size = ROUND8(alloc_size);
    void * mem = qmalloc(alloc_size);
    InitMemPoolBlockHeader(mem,-1);
    //
    PoolBlockHeader* ppbh = reinterpret_cast<PoolBlockHeader*>(mem);
    ppbh->sys_alloc_type_ = INT32(AT_SYSTEM);
    ppbh->sys_alloc_state_ = AS_ALLOCING;
    ppbh->sys_alloc_size_ = alloc_size;

    void * retpos = MoveBackward(mem,PBHEADER_SIZE);
    BlockHeader* pbh = reinterpret_cast<BlockHeader*>(retpos);
    pbh->diff_ = PBHEADER_SIZE;
    pbh->size_ = sz;
    return MoveBackward(retpos,BHEADER_SIZE);
}

void MsgMemPool::SystemFree(PoolBlockHeader* ppbh)
{
    assert(ppbh->ref_ == -1);
    assert(ppbh->sys_alloc_state_ == AS_ALLOCING);

    allocing_.erase((void*)ppbh);

    qfree(ppbh);
}
//2-pool alloc/free;
void* MsgMemPool::PoolAlloc(int32_t sz)
{
    //1-变量声明;
    int32_t need_size = ROUND8(sz + BHEADER_SIZE);
    PoolBlockHeader* ppbh = NULL;
    //2-find the PoolBlockHeader
    ppbh = GetFromAllocing(need_size);
    //2-1如果allocing_没有,那么到freed_中去拿.
    if (!ppbh)
    {
        ppbh = GetFromFreed();
    }
    assert(ppbh);
    //3-find the block;
    void *ret_mem = MoveBackward((void*)(ppbh),ppbh->pos_alloced_);

    BlockHeader* pbh = reinterpret_cast<BlockHeader*>(ret_mem);
    pbh->diff_ = ppbh->pos_alloced_;
    pbh->size_ = need_size;

    ppbh->pos_alloced_ += need_size;
    ++ppbh->cnt_alloced_;
    ppbh->sz_alloced_ += need_size;

    return MoveBackward(ret_mem,BHEADER_SIZE);
}

PoolBlockHeader* MsgMemPool::GetFromAllocing(int32_t need_size)
{
    PoolBlockHeader* ppbh = NULL;
    PoolBlockHeader* ptmp = NULL;
    vector<void*> temp;
    int32_t left_size = 0;
    MItr need_move = allocing_.end();
    MItr pos = allocing_.begin();
    MItr end = allocing_.end();
    while (pos != end)
    {
        ptmp = reinterpret_cast<PoolBlockHeader*>(*pos);
        left_size = ptmp->sys_alloc_size_ - ptmp->pos_alloced_;
        assert(left_size >= 0);
        if (left_size >= need_size)
        {
            ppbh = ptmp;
            break;
        }
        //FUCK TODO:如果剩余内存空间太小,直接移到freeing_队列中去.
        NeedMove(left_size,*pos,temp);
        ++pos;
    }
    MoveToFree(temp);
    return ppbh;
}

void MsgMemPool::MoveToFree(vector<void*> &temp)
{
    if (temp.empty())
    {
        return ;
    }
    size_t pos = 0;
    size_t tsize = temp.size();
    void *mem = NULL;
    PoolBlockHeader* ppbh = NULL;
    while (pos != tsize)
    {
        mem = temp.at(pos++);
        freeing_.insert(mem);
        ppbh = reinterpret_cast<PoolBlockHeader*>(mem);
        ppbh->sys_alloc_state_ = INT32(AS_FREEING);
        allocing_.erase(mem);
    }
    temp.clear();
}

void MsgMemPool::NeedMove(int32_t left_size,void *mem,vector<void*> &temp)
{
    //一次,最多操作ONE_TIME_PAGE_MOVE_NUM;
    if (temp.size()> ONE_TIME_PAGE_MOVE_NUM)
    {
        return ;
    }
    //内存剩余空间足够(>128byte);
    if (left_size > FREE_GUARD_SIZE)
    {
        return ;
    }
    //内存剩余空间太小(<32byte);
    if (left_size < FREE_MIN_SIZE)
    {
        temp.push_back(mem);
        return ;
    }
    //内存空间,32~128字节之间.
    int32_t tmp = 1*(freed_.size() + freeing_.size());
    if (allocing_.size() - temp.size() < tmp)
    {
        return ;
    }
    temp.push_back(mem);
}

PoolBlockHeader* MsgMemPool::GetFromFreed()
{
    if (freed_.empty())
    {
        Resize();
    }
    assert(!freed_.empty());
    PoolBlockHeader* ppbh = reinterpret_cast<PoolBlockHeader*>(freed_.back());
    ppbh->sys_alloc_state_ = INT32(AS_ALLOCING);
    freed_.pop_back();
    allocing_.insert((void*)ppbh);
    return ppbh;
}

void MsgMemPool::PoolFree(PoolBlockHeader* ppbh,BlockHeader* pbh)
{
    ++ppbh->cnt_freed_;
    ppbh->sz_freed_ += pbh->size_;
    if (ppbh->cnt_alloced_ != ppbh->cnt_freed_)
    {
        return ;
    }
    //已经全部释放,那么可以还给freed_队列了.
    //ppbh->cnt_alloced_ == ppbh->cnt_freed_
    assert(ppbh->sz_alloced_ == ppbh->sz_freed_);
    switch(ppbh->sys_alloc_state_)
    {
    case AS_ALLOCING :
    {
        allocing_.erase((void*)ppbh);
        break;
    }
    case AS_FREEING:
    {
        freeing_.erase((void*)ppbh);
        break;
    }
    case AS_FREED://这状态的PoolBlockHeader是不可能存在的.
    default :
    {
        assert(0);
    }
    }
    //
    InitMemPoolBlockHeader((void*)ppbh,ppbh->ref_);
    freed_.push_back((void*)ppbh);
    FreeToSystem();
}

void MsgMemPool::FreeToSystem()
{
    int32_t inused = INT32(allocing_.size() + freeing_.size());
    int32_t notused = INT32(freed_.size());
    int32_t diff = ONE_TIME_ALLOC_NUMBER*2;
    if (notused - inused < diff)
    {
        return ;
    }
    //
    void *p = freed_.back();
    freed_.pop_back();
    std::vector<void* >::iterator cpos,cend,last;
    cpos = mem_pool_.begin();
    cend = mem_pool_.end();
    last = cend-1;
    while (cpos != cend)
    {
        if (*cpos == p)
        {
            break;
        }
        ++cpos;
    }
    assert (cpos != cend);
    *cpos = *last;
    mem_pool_.pop_back();
    free(p);
}


//3-辅助函数.
void MsgMemPool::Resize()
{
    int32_t new_size = mem_pool_.size() + ONE_TIME_ALLOC_NUMBER;
    //mem_pool_.resize(new_size);
    //1-alloc memory;
    void *ptemp = NULL;
    for (int32_t n=0; n!=ONE_TIME_ALLOC_NUMBER;++n)
    {
        ptemp = qmalloc(ONE_PAGE_IN_POOL);
        assert(ptemp);
        //init node;
        InitMemPoolBlockHeader(ptemp,INT32(mem_pool_.size()));
        mem_pool_.push_back(ptemp);
        freed_.push_back(ptemp);
    }
}

void MsgMemPool::InitMemPoolBlockHeader(void *mem,int32_t ref)
{
    PoolBlockHeader* ppbh = reinterpret_cast<PoolBlockHeader*>(mem);
    memset(mem,0,sizeof(PoolBlockHeader));
    ppbh->ref_ = ref;
    ppbh->sys_alloc_size_ = ONE_PAGE_IN_POOL;
    ppbh->sys_alloc_state_ = INT16(AS_FREED);
    ppbh->sys_alloc_type_ = INT16(AT_POOL);
    //
    ppbh->pos_alloced_ = PBHEADER_SIZE;
}




































