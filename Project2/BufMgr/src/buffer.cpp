/**
 * @author Avichal Rakesh (9072949259)
 * @author Shyamal Anadkat (9071804893)
 * @author Bryce Sprecher (9061820008)
 *
 * This serves as the buffer pool management class in this database.
 *
 * Page usage is tracked via pin counts. Reading, allocating and
 * and disposal of buffer pool pages is handled by this class.
 * It implements a clock replacement policy.
 *
 */

#include <memory>
#include <iostream>
#include "buffer.h"
#include "exceptions/buffer_exceeded_exception.h"
#include "exceptions/page_not_pinned_exception.h"
#include "exceptions/page_pinned_exception.h"
#include "exceptions/bad_buffer_exception.h"
#include "exceptions/hash_not_found_exception.h"

namespace badgerdb {
BufMgr::BufMgr(std::uint32_t bufs) : numBufs(bufs)
{
  bufDescTable = new BufDesc[bufs];

  for (FrameId i = 0; i < bufs; i++)
  {
    bufDescTable[i].frameNo = i;
    bufDescTable[i].valid   = false;
  }

  bufPool = new Page[bufs];

  int htsize = ((((int)(bufs * 1.2)) * 2) / 2) + 1;
  hashTable = new BufHashTbl(htsize); // allocate the buffer hash table

  clockHand = bufs - 1;
}

BufMgr::~BufMgr()
{
  // Write back all dirty pages that are also valid
  for (FrameId i = 0; i < numBufs; i++) 
  {
    if(bufDescTable[i].dirty && bufDescTable[i].valid) 
    {
      bufDescTable[i].file ->
      writePage(bufPool[i]);
      bufDescTable[i].dirty = false;
    }
  }

  delete[] bufPool;
  delete[] bufDescTable;
  delete hashTable;
}

void BufMgr::advanceClock()
{
  // Modulo on number of buffer frames to get index (frame ID)
  clockHand = (clockHand + 1) % numBufs;
}

void BufMgr::allocBuf(FrameId& frame)
{
  // Used to store the number of currently pinned frames in the buffer
  std::uint32_t numPinned = 0;

  bool found = false;

  do
  {
    BufDesc* frame_info = &bufDescTable[clockHand];

    if (!frame_info->valid)
    {
      found = true;
      frame = clockHand;
    }
    else if (frame_info->pinCnt)
    {
      numPinned++;
      advanceClock();
    }
    else if (frame_info->refbit)
    {
      frame_info->refbit = 0;
      advanceClock();
    }
    else // case of valid frame be written to disk
    {
      hashTable->remove(frame_info->file, frame_info->pageNo);

      if (frame_info->dirty)
      {
        frame_info->file->writePage(bufPool[clockHand]);
      }

      frame_info->Clear();
      found = true;
    }
  }while (!found && numPinned < numBufs);

  // If all buffer frames are pinned throw exception
  if (numPinned == numBufs)
  {
    throw BufferExceededException();
  }

  frame = clockHand;
}

void BufMgr::readPage(File *file, const PageId pageNo, Page *& page)
{
  // Case 2: Page is present in buffer pool
  try
  {
    FrameId read_target_frame_number;

    // if present in hashTable read_target_frame_number gets frame number
    // otherwise a HashNotFoundException will be thrown
    hashTable->lookup(file, pageNo, read_target_frame_number);

    // set refBit to true because this page is being referenced
    bufDescTable[read_target_frame_number].refbit = true;

    // increment pinCount because page is being read
    bufDescTable[read_target_frame_number].pinCnt++;

    // return pointer to frame where page is stored
    page = &bufPool[read_target_frame_number];
  }
  // Case 1: Page is not in buffer pool
  catch (HashNotFoundException& hnfe)
  {
    // New frame allocated from buffer pool and page stored in new frame
    FrameId read_target_frame_number;
    allocBuf(read_target_frame_number);
    bufPool[read_target_frame_number] = file->readPage(pageNo);

    // Instert frame into hashTable
    hashTable->insert(file, pageNo, read_target_frame_number);
    bufDescTable[read_target_frame_number].Set(file, pageNo);

    // return pointer to frame where page is stored
    page = &bufPool[read_target_frame_number];

    // HashTableException (optional) ?
  }
}

void BufMgr::unPinPage(File *file, const PageId pageNo, const bool dirty)
{
  FrameId unpinned_frame_number;

  try
  {
    hashTable->lookup(file, pageNo, unpinned_frame_number);

    // Page has no pins, throw exception
    if (bufDescTable[unpinned_frame_number].pinCnt == 0)
    {
      throw PageNotPinnedException(file->filename(), pageNo, unpinned_frame_number);
    }

    // Decrement pin count
    bufDescTable[unpinned_frame_number].pinCnt--;

    // WHY is the refbit true only if new pinCnt = 0? why not <= 0?
    if (bufDescTable[unpinned_frame_number].pinCnt == 0)
    {
      bufDescTable[unpinned_frame_number].refbit = true;
    }

    if (dirty)
    {
      bufDescTable[unpinned_frame_number].dirty = true;
    }
  }
  catch (HashNotFoundException hnfe)
  {
    // No corresponding frame found.
    // do nothing
  }
}

void BufMgr::allocPage(File *file, PageId& pageNo, Page *& page)
{
  //Create a new page assigining it a page number
  Page *new_page = new Page();
  *new_page = file->allocatePage();
  pageNo = new_page->page_number();
  
  //Allocate frame and add page to hashTable
  FrameId id;
  allocBuf(id);
  hashTable->insert(file, pageNo, id);
  bufDescTable[id].Set(file, pageNo);
  bufPool[id] = *new_page;    
  page = &bufPool[id];
}

void BufMgr::disposePage(File *file, const PageId PageNo)
{
  try
  {
    //Locate frame the page is in through hashTable
    FrameId frameNum;
    hashTable->lookup(file, PageNo, frameNum);
    BufDesc* target_frame_desc = &bufDescTable[frameNum];

    //If frame is in use, ie pinned, can't dispose
    if(bufDescTable[frameNum].pinCnt != 0) 
    {
      throw PagePinnedException
      (target_frame_desc->file->filename(), target_frame_desc->pageNo, frameNum);
    }

    //Remove frame description from table and hashTable
    bufDescTable[frameNum].Clear();
    hashTable->remove(file, PageNo);
  }
  catch (HashNotFoundException e)
  {
    // Page not in the buffer.
    // Nothing more to be done to the buffer.
    // Ignore the exception.
  }

  file->deletePage(PageNo);
}

void BufMgr::flushFile(const File *file)
{
  for (FrameId i = 0; i < numBufs; i++)
  {
    BufDesc* target_frame = &bufDescTable[i];

    // Any buffer frame with a page from this file must be freed and flushed
    if ((target_frame->file) == file)
    {
      //File in use if pinned
      if (target_frame->pinCnt > 0)
      {
        throw PagePinnedException(target_frame->file->filename(), target_frame->pageNo, i);
      }
      //File only candidated to be flushed if valid
      if (!target_frame->valid)
      {
        throw BadBufferException(i, target_frame->dirty, false, target_frame->refbit);
      }
      //Only frames that are dirty need to be written to disk
      if (target_frame->dirty)
      {
        target_frame->file->writePage(bufPool[i]);
      }

      //Any frame belonging to the file is removed from hashTable and cleared
      hashTable->remove(target_frame->file, target_frame->pageNo);
      target_frame->Clear();
    }
  }
}

void BufMgr::printSelf(void)
{
  BufDesc *tmpbuf;
  int validFrames = 0;

  for (std::uint32_t i = 0; i < numBufs; i++)
  {
    tmpbuf = &(bufDescTable[i]);
    std::cout << "FrameNo:" << i << " ";
    tmpbuf->Print();

    if (tmpbuf->valid == true)
    {
      validFrames++;
    }
  }

  std::cout << "Total Number of Valid Frames:" << validFrames << "\n";
}
}
