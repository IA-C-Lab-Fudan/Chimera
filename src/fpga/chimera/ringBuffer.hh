#ifndef __FPGA_CHIMERA_RINGBUFFER_HH__
#define __FPGA_CHIMERA_RINGBUFFER_HH__

#include <vector>

namespace gem5
{
namespace fpga
{

template <typename T>
class RingBuffer
{
  private:
    int            m_depth;
    std::vector<T> m_queue;

    int m_cons_head;
    int m_cons_tail;
    int m_prod_head;
    int m_prod_tail;

  public:
    RingBuffer(int depth) : m_depth(depth + 1), m_cons_head(0), m_cons_tail(0), m_prod_head(0), m_prod_tail(0)
    {
        m_queue.resize(m_depth + 1);
    }
    ~RingBuffer()
    {
    }

    bool isFull()
    {
        return ((m_prod_head + 1) % m_depth == m_cons_tail ? true : false);
    }

    bool isEmpty()
    {
        return (m_cons_head == m_prod_tail ? true : false);
    }

    bool enqueue(T obj)
    {
        if (!isFull()) {
            int local_prod_head = m_prod_head;
            int local_prod_next = (local_prod_head + 1) % m_depth;

            m_prod_head                 = local_prod_next;
            m_queue.at(local_prod_head) = obj;
            m_prod_tail                 = m_prod_head;
            return true;
        } else {
            return false;
        }
    }

    T dequeue()
    {
        int local_cons_head = m_cons_head;
        int local_cons_next = (local_cons_head + 1) % m_depth;

        m_cons_head = local_cons_next;
        T result    = m_queue.at(local_cons_head);
        m_cons_tail = m_cons_head;
        return result;
    }

    T peek()
    {
        int local_cons_head = m_cons_head;
        T   result          = m_queue.at(local_cons_head);
        return result;
    }

    int size()
    {
        return (m_prod_tail - m_cons_head) >= 0 ? (m_prod_tail - m_cons_head) : (m_prod_tail - m_cons_head + m_depth);
    }
};

} // namespace fpga
} // namespace gem5


#endif