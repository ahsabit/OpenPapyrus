/// \file       outqueue.h
/// \brief      Output queue handling in multithreaded coding
//  Author:     Lasse Collin
//  This file has been put into the public domain. You can do whatever you want with this file.

//
// Descr: Output buffer for a single thread
//
struct lzma_outbuf {
	uint8 * buf; /// Pointer to the output buffer of lzma_outq.buf_size_max bytes
	size_t size; /// Amount of data written to buf
	/// Additional size information
	lzma_vli unpadded_size;
	lzma_vli uncompressed_size;
	/// True when no more data will be written into this buffer.
	///
	/// \note       This is read by another thread and thus access
	///             to this variable needs a mutex.
	bool finished;
};

struct lzma_outq {
	lzma_outbuf * bufs; /// Array of buffers that are used cyclically.
	uint8 * bufs_mem; /// Memory allocated for all the buffers
	size_t buf_size_max; /// Amount of buffer space available in each buffer
	uint32_t bufs_allocated; /// Number of buffers allocated
	uint32_t bufs_pos; /// Position in the bufs array. The next buffer to be taken into use is bufs[bufs_pos].
	uint32_t bufs_used; /// Number of buffers in use
	size_t read_pos; /// Position in the buffer in lzma_outq_read()
};

/**
 * \brief       Calculate the memory usage of an output queue
 *
 * \return      Approximate memory usage in bytes or UINT64_MAX on error.
 */
extern  uint64 lzma_outq_memusage( uint64 buf_size_max, uint32_t threads);

/// \brief      Initialize an output queue
///
/// \param      outq            Pointer to an output queue. Before calling
///                             this function the first time, *outq should
///                             have been zeroed with memzero() so that this
///                             function knows that there are no previous
///                             allocations to free.
/// \param      allocator       Pointer to allocator or NULL
/// \param      buf_size_max    Maximum amount of data that a single buffer
///                             in the queue may need to store.
/// \param      threads         Number of buffers that may be in use
///                             concurrently. Note that more than this number
///                             of buffers will actually get allocated to
///                             improve performance when buffers finish
///                             out of order.
///
/// \return     - LZMA_OK
///             - LZMA_MEM_ERROR
///
extern lzma_ret lzma_outq_init(lzma_outq * outq, const lzma_allocator * allocator,  uint64 buf_size_max, uint32_t threads);
/// \brief      Free the memory associated with the output queue
extern void lzma_outq_end(lzma_outq * outq, const lzma_allocator * allocator);

/// \brief      Get a new buffer
///
/// lzma_outq_has_buf() must be used to check that there is a buffer
/// available before calling lzma_outq_get_buf().
///
extern lzma_outbuf * lzma_outq_get_buf(lzma_outq * outq);

/// \brief      Test if there is data ready to be read
///
/// Call to this function must be protected with the same mutex that
/// is used to protect lzma_outbuf.finished.
///
extern bool lzma_outq_is_readable(const lzma_outq * outq);

/// \brief      Read finished data
///
/// \param      outq            Pointer to an output queue
/// \param      out             Beginning of the output buffer
/// \param      out_pos         The next byte will be written to
///                             out[*out_pos].
/// \param      out_size        Size of the out buffer; the first byte into
///                             which no data is written to is out[out_size].
/// \param      unpadded_size   Unpadded Size from the Block encoder
/// \param      uncompressed_size Uncompressed Size from the Block encoder
///
/// \return     - LZMA: All OK. Either no data was available or the buffer
///               being read didn't become empty yet.
///             - LZMA_STREAM_END: The buffer being read was finished.
///               *unpadded_size and *uncompressed_size were set.
///
/// \note       This reads lzma_outbuf.finished variables and thus call
///             to this function needs to be protected with a mutex.
///
extern lzma_ret lzma_outq_read(lzma_outq * outq, uint8 * out, size_t * out_pos,
    size_t out_size, lzma_vli * unpadded_size, lzma_vli * uncompressed_size);
/// \brief      Test if there is at least one buffer free
///
/// This must be used before getting a new buffer with lzma_outq_get_buf().
///
static inline bool lzma_outq_has_buf(const lzma_outq * outq)
{
	return outq->bufs_used < outq->bufs_allocated;
}
/// \brief      Test if the queue is completely empty
static inline bool lzma_outq_is_empty(const lzma_outq * outq)
{
	return outq->bufs_used == 0;
}
